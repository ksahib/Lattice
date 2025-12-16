#include <grpcpp/grpcpp.h>
#include "task_service.grpc.pb.h"
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <cstring>
#include <iostream>
#include <map>
#include <chrono>
#include <fstream>
#include <json/json.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using task::TaskService;
using task::TaskRequest;
using task::TaskResponse;

// Keep same function signature
typedef void (*loop_body_fn)(int64_t i, void *env);

// Worker channel cache (address -> channel mapping)
static std::map<std::string, std::shared_ptr<Channel>> worker_channel_cache;
static std::mutex channel_cache_mutex;

// Environment size (set by LLVM pass and written to /tmp/env_struct_size.txt)
static uint64_t g_current_env_size = 0;

// Read environment size from file written by loop outliner pass
static uint64_t read_env_size_from_file() {
    const char* path = "/tmp/env_struct_size.txt";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "env size: cannot open " << path << ", using 0\n";
        return 0; // will trigger fallback
    }
    uint64_t last = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        try {
            if (!line.empty()) last = std::stoull(line);
        } catch (...) {
            // ignore bad lines
        }
    }
    if (last == 0)
        std::cerr << "env size: no valid size found, using 0\n";
    else
        std::cerr << "env size: " << last << "\n";
    return last;
}

// Read current outlined IR from path provided via env var LATTICE_CURRENT_IR
static std::string read_ir_from_env() {
    const char* ir_path = std::getenv("LATTICE_CURRENT_IR");
    if (!ir_path) {
        std::cerr << "LATTICE_CURRENT_IR not set; no IR will be sent\n";
        return "";
    }
    std::ifstream ir_file(ir_path, std::ios::binary);
    if (!ir_file.is_open()) {
        std::cerr << "Failed to open IR file: " << ir_path << "\n";
        return "";
    }
    std::string contents((std::istreambuf_iterator<char>(ir_file)),
                         std::istreambuf_iterator<char>());
    return contents;
}

std::vector<std::string> load_worker_addresses_from_file(const std::string& file_path = "./workers.json") {
    std::vector<std::string> addresses;
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open workers.json file\n";
        return addresses;
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    if (Json::parseFromStream(builder, file, &root, &errors)) {
        if (root.isMember("workers") && root["workers"].isArray()) {
            for (const auto& worker_obj : root["workers"]) {
                if (worker_obj.isMember("address")) {
                    std::string address = worker_obj["address"].asString();
                    std::string status = "available";
                    if (worker_obj.isMember("status")) {
                        status = worker_obj["status"].asString();
                    }
                    // Only include available workers
                    if (status == "available") {
                        addresses.push_back(address);
                    }
                }
            }
        }
    }
    file.close();
    
    return addresses;
}

std::shared_ptr<Channel> get_or_create_channel(const std::string& address) {
    std::lock_guard<std::mutex> lock(channel_cache_mutex);
    
    auto it = worker_channel_cache.find(address);
    if (it != worker_channel_cache.end()) {
        return it->second;
    }
    
    // Create new channel
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    worker_channel_cache[address] = channel;
    return channel;
}

// Serialize environment to bytes
std::string serialize_environment(void* env, size_t env_size) {
    if (!env || env_size == 0) return "";

    return std::string(static_cast<char*>(env), env_size);
}

// Deserialize environment from bytes
void* deserialize_environment(const std::string& data, size_t size) {
    if (data.empty() || size == 0) return nullptr;
    void* env = malloc(size);
    if (env) {
        memcpy(env, data.data(), std::min(data.size(), size));
    }
    return env;
}

// Partition iteration range across workers
struct TaskPartition {
    int64_t start;
    int64_t end;
    int64_t step;
    std::string worker_address;  // Changed from worker_index to address
};

std::vector<TaskPartition> partition_range(
    int64_t start, int64_t end, int64_t step, 
    const std::vector<std::string>& worker_addresses) {
    
    std::vector<TaskPartition> partitions;
    size_t num_workers = worker_addresses.size();
    if (num_workers == 0) return partitions;
    
    int64_t total_iterations = (end - start) / step;
    if (total_iterations <= 0) return partitions;
    
    int64_t iterations_per_worker = total_iterations / num_workers;
    int64_t remainder = total_iterations % num_workers;
    
    int64_t current_start = start;
    for (size_t i = 0; i < num_workers && current_start < end; i++) {
        TaskPartition p;
        p.start = current_start;
        p.step = step;
        p.worker_address = worker_addresses[i % num_workers];  // Round-robin
        
        int64_t iterations = iterations_per_worker;
        if (i < remainder) iterations++;
        
        p.end = current_start + (iterations * step);
        if (p.end > end) p.end = end;
        
        partitions.push_back(p);
        current_start = p.end;
    }
    
    return partitions;
}

// Execute task on a single worker
bool execute_task_on_worker(
    const std::string& worker_address,
    int64_t start, int64_t end, int64_t step,
    void* env, size_t env_size,
    int64_t task_id,
    void** result_env, size_t* result_size) {
    
    // Get or create channel for this worker
    auto channel = get_or_create_channel(worker_address);
    auto stub = TaskService::NewStub(channel);
    TaskRequest request;
    request.set_task_id(task_id);
    request.set_start(start);
    request.set_end(end);
    request.set_step(step);
    request.set_environment_data(serialize_environment(env, env_size));
    request.set_environment_size(env_size);
    request.set_function_name("outlined_main_loopbody");

    // Attach current outlined IR so the worker can compile it locally
    std::string ir_data = read_ir_from_env();
    if (!ir_data.empty()) {
        request.set_ir_data(ir_data);
        request.set_ir_format("ll"); // we send textual LLVM IR (.opt.ll)
    }
    
    TaskResponse response;
    ClientContext context;
    
    // Set timeout
    context.set_deadline(std::chrono::system_clock::now() + 
                        std::chrono::seconds(60));
    
    Status status = stub->ExecuteTask(&context, request, &response);
    
    if (status.ok() && response.success()) {
        // Deserialize result
        if (response.result_size() > 0 && !response.result_data().empty()) {
            *result_env = deserialize_environment(
                response.result_data(), 
                response.result_size());
            *result_size = response.result_size();
        } else {
            *result_env = nullptr;
            *result_size = 0;
        }
        return true;
    } else {
        std::cerr << "Task " << task_id << " failed: " 
                  << (status.ok() ? response.error_message() : status.error_message())
                  << std::endl;
        *result_env = nullptr;
        *result_size = 0;
        return false;
    }
}

// Merge results from multiple workers
void merge_results(
    const std::vector<void*>& result_envs,
    const std::vector<size_t>& result_sizes,
    void* original_env,
    size_t original_env_size) {
    
    if (result_envs.empty()) return;
    
    // For now, simple merge: use the last result
    // TODO: Implement proper merging based on data structure
    // For arrays: merge by index
    // For scalars: use reduction operation
    
    if (result_envs.back() && result_sizes.back() == original_env_size) {
        memcpy(original_env, result_envs.back(), original_env_size);
    }
    
    // Free temporary result buffers
    for (void* env : result_envs) {
        if (env) free(env);
    }
}

// NEW: gRPC-based parallel_for_runtime
extern "C" void parallel_for_runtime(
    int64_t start, int64_t end, int64_t step, 
    loop_body_fn body, void *env) {
    
    if (step == 0 || !body) return;
    
    // Get environment size (in bytes) for the outlined environment struct.
    // The loop outliner pass writes this to /tmp/env_struct_size.txt.
    g_current_env_size = read_env_size_from_file();
    if (g_current_env_size == 0) {
        std::cerr << "Warning: Environment size is 0, using fallback\n";
        g_current_env_size = 1024;  // Fallback
    }
    
    // Load available workers from workers.json file
    std::vector<std::string> worker_addresses = load_worker_addresses_from_file();
    
    if (worker_addresses.empty()) {
        std::cerr << "No available workers found, skipping distributed execution\n";
        return;
    }
    
    // Partition the iteration range across available workers
    auto partitions = partition_range(start, end, step, worker_addresses);
    
    if (partitions.empty()) {
        std::cerr << "No partitions created\n";
        return;
    }
    
    // Execute tasks in parallel using threads
    std::vector<std::thread> threads;
    std::vector<bool> results(partitions.size());
    std::vector<void*> result_envs(partitions.size(), nullptr);
    std::vector<size_t> result_sizes(partitions.size(), 0);
    std::vector<std::string> worker_addresses_used(partitions.size());
    std::mutex results_mutex;
    
    for (size_t i = 0; i < partitions.size(); i++) {
        threads.emplace_back([&, i]() {
            auto& partition = partitions[i];
            worker_addresses_used[i] = partition.worker_address;
            
            void* result_env = nullptr;
            size_t result_size = 0;
            
            results[i] = execute_task_on_worker(
                partition.worker_address,
                partition.start, partition.end, partition.step,
                env, g_current_env_size,
                static_cast<int64_t>(i),  // task_id
                &result_env, &result_size
            );
            
            // Store results
            std::lock_guard<std::mutex> lock(results_mutex);
            result_envs[i] = result_env;
            result_sizes[i] = result_size;
        });
    }
    
    // Wait for all tasks
    for (auto& t : threads) {
        t.join();
    }
    
    // Merge results
    merge_results(result_envs, result_sizes, env, g_current_env_size);
    
    // Check if all succeeded
    bool all_success = true;
    for (bool r : results) {
        if (!r) all_success = false;
    }
    
    if (!all_success) {
        std::cerr << "Warning: Some tasks failed\n";
    } else {
        std::cerr << "All tasks completed successfully\n";
    }
}