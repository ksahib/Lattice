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

// Metadata about env struct fields
struct EnvFieldMeta {
    int index = -1;
    uint64_t offset = 0;
    std::string kind; // "SCALAR", "POINTER_ARRAY", "FIXED_ARRAY", "SCALAR_PTR", ...
    uint64_t elem_size = 0;
    int len_field = -1; // index of length field (scalar), or -1 if unknown
    int64_t fixed_length = -1; // for fixed-size arrays, else -1
};

static std::vector<EnvFieldMeta> g_env_fields;
static uint64_t g_env_struct_size = 0;
static std::once_flag g_env_meta_once;

// Forward declaration so debug_dump_env can call it before its full definition.
static void load_env_metadata();

// Debug helper to pretty-print env contents using metadata.
static void debug_dump_env(const std::string& label, void* env, size_t env_size) {
    if (!env || env_size == 0) {
        std::cerr << "[LATTICE] " << label << ": env is null or size=0\n";
        return;
    }

    load_env_metadata();
    if (g_env_fields.empty()) {
        std::cerr << "[LATTICE] " << label << ": no env metadata loaded\n";
        return;
    }

    std::cerr << "[LATTICE] " << label << ": env dump (size=" << env_size << ")\n";

    for (const auto& meta : g_env_fields) {
        // Dump FIXED_ARRAY of int32_t
        if (meta.kind == "FIXED_ARRAY" &&
            meta.fixed_length > 0 &&
            meta.elem_size == 4) {
            if (meta.offset + sizeof(uintptr_t) > env_size) continue;

            uintptr_t ptr_val = 0;
            memcpy(&ptr_val,
                   static_cast<char*>(env) + meta.offset,
                   sizeof(uintptr_t));
            if (ptr_val == 0) continue;

            auto* arr = reinterpret_cast<const int32_t*>(ptr_val);
            uint64_t len = static_cast<uint64_t>(meta.fixed_length);

            std::cerr << "  [field " << meta.index << "] FIXED_ARRAY<int32>("
                      << "len=" << len << "):";
            for (uint64_t i = 0; i < len; ++i) {
                std::cerr << " " << arr[i];
            }
            std::cerr << "\n";
        }
        // Dump SCALAR_PTR assumed as int64_t
        else if (meta.kind == "SCALAR_PTR" &&
                 meta.elem_size == 8) {
            if (meta.offset + sizeof(uintptr_t) > env_size) continue;

            uintptr_t ptr_val = 0;
            memcpy(&ptr_val,
                   static_cast<char*>(env) + meta.offset,
                   sizeof(uintptr_t));
            if (ptr_val == 0) continue;

            auto* p = reinterpret_cast<const int64_t*>(ptr_val);
            std::cerr << "  [field " << meta.index << "] SCALAR_PTR<int64>: "
                      << *p << "\n";
        }
    }
}

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

// Read env metadata JSON emitted by the outliner
static void load_env_metadata() {
    std::call_once(g_env_meta_once, []() {
        // Allow overriding via ENV_METADATA_PATH; otherwise default to Worker folder.
        const char* env_override = std::getenv("ENV_METADATA_PATH");
        const char* default_path = "/home/niloy/vs_code/course/cse299/Lattice/Worker/env_metadata.json";
        const char* path = (env_override && *env_override) ? env_override : default_path;
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            std::cerr << "env meta: cannot open " << path << "\n";
            return;
        }
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
            std::cerr << "env meta: parse failed: " << errs << "\n";
            return;
        }
        if (root.isMember("struct_size"))
            g_env_struct_size = root["struct_size"].asUInt64();
        if (root.isMember("fields") && root["fields"].isArray()) {
            for (const auto& f : root["fields"]) {
                EnvFieldMeta m;
                if (f.isMember("index")) m.index = f["index"].asInt();
                if (f.isMember("offset")) m.offset = f["offset"].asUInt64();
                if (f.isMember("kind")) m.kind = f["kind"].asString();
                if (f.isMember("elem_size")) m.elem_size = f["elem_size"].asUInt64();
                if (f.isMember("len_field")) m.len_field = f["len_field"].asInt();
                if (f.isMember("fixed_length")) m.fixed_length = f["fixed_length"].asInt64();
                g_env_fields.push_back(m);
            }
        }
    });
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
    void** result_env, size_t* result_size,
    std::vector<task::EnvArrayField>* out_arrays) {
    
    // Get or create channel for this worker
    auto channel = get_or_create_channel(worker_address);
    auto stub = TaskService::NewStub(channel);
    TaskRequest request;
    request.set_task_id(task_id);
    request.set_start(start);
    request.set_end(end);
    request.set_step(step);

    // Debug: show env before sending to this worker/partition
    {
        std::stringstream label;
        label << "master before task " << task_id
              << " [worker=" << worker_address
              << ", range=" << start << ":" << end << ":" << step << "]";
        debug_dump_env(label.str(), env, env_size);
    }

    request.set_environment_data(serialize_environment(env, env_size));
    request.set_environment_size(env_size);
    // Call the LLVM-generated wrapper(i64, i8*) instead of the raw outlined function.
    request.set_function_name("wrapper");

    // Attach current outlined IR so the worker can compile it locally
    std::string ir_data = read_ir_from_env();
    if (!ir_data.empty()) {
        request.set_ir_data(ir_data);
        request.set_ir_format("ll"); // we send textual LLVM IR (.opt.ll)
    }

    // Attach env metadata JSON so worker knows env structure (for cross-PC execution)
    load_env_metadata();
    const char* env_override = std::getenv("ENV_METADATA_PATH");
    const char* default_path = "/home/niloy/vs_code/course/cse299/Lattice/Worker/env_metadata.json";
    const char* meta_path = (env_override && *env_override) ? env_override : default_path;
    
    std::ifstream meta_file(meta_path);
    if (meta_file.is_open()) {
        std::string meta_json((std::istreambuf_iterator<char>(meta_file)),
                              std::istreambuf_iterator<char>());
        request.set_env_metadata_json(meta_json);
        meta_file.close();
    } else {
        std::cerr << "Warning: Could not read env_metadata.json from " << meta_path << "\n";
    }

    // Attach logical array payloads using env metadata
    // Attach arrays/buffers for POINTER_ARRAY, FIXED_ARRAY and SCALAR_PTR fields
    for (const auto& meta : g_env_fields) {
        if (meta.elem_size == 0) continue;

        uint64_t len = 0;
        if (meta.kind == "POINTER_ARRAY") {
            if (meta.len_field < 0) continue;
            auto len_it = std::find_if(g_env_fields.begin(), g_env_fields.end(),
                                       [&](const EnvFieldMeta& m){ return m.index == meta.len_field; });
            if (len_it != g_env_fields.end()) {
                size_t len_off = len_it->offset;
                if (len_off + sizeof(uint64_t) <= env_size) {
                    memcpy(&len, static_cast<char*>(env) + len_off, sizeof(uint64_t));
                }
            }
        } else if (meta.kind == "FIXED_ARRAY") {
            if (meta.fixed_length > 0) len = static_cast<uint64_t>(meta.fixed_length);
        } else if (meta.kind == "SCALAR_PTR") {
            // Single scalar pointed-to value; treat as length-1 buffer
            len = 1;
        } else {
            continue;
        }

        if (len == 0) continue;

        // Read pointer value from env at meta.offset
        uintptr_t ptr_val = 0;
        if (meta.offset + sizeof(uintptr_t) <= env_size) {
            memcpy(&ptr_val, static_cast<char*>(env) + meta.offset, sizeof(uintptr_t));
        }
        if (ptr_val == 0) continue;

        const char* src = reinterpret_cast<const char*>(ptr_val);
        uint64_t bytes = len * meta.elem_size;

        try {
            task::EnvArrayField* arr = request.add_arrays();
            arr->set_field_index(meta.index);
            arr->set_length(len);
            arr->set_data(src, bytes);
        } catch (...) {
            std::cerr << "env meta: failed to append array payload for field " << meta.index << "\n";
        }
    }
    
    TaskResponse response;
    ClientContext context;
    
    // Set timeout
    context.set_deadline(std::chrono::system_clock::now() + 
                        std::chrono::seconds(60));
    
    Status status = stub->ExecuteTask(&context, request, &response);
    
    if (status.ok() && response.success()) {
        // Capture array payloads from worker (safe for merging)
        if (out_arrays) {
            out_arrays->clear();
            for (int i = 0; i < response.arrays_size(); ++i) {
                out_arrays->push_back(response.arrays(i));
            }
        }

        // Deserialize result (kept for compatibility, but merge will use arrays)
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

// Merge results from multiple workers using array payloads (safe: no pointer dereferencing)
void merge_results(
    const std::vector<std::vector<task::EnvArrayField>>& all_arrays,
    const std::vector<TaskPartition>& partitions,
    void* original_env,
    size_t original_env_size) {
    
    if (all_arrays.empty() || partitions.empty()) return;

    load_env_metadata();

    // Build quick lookup: field_index -> EnvFieldMeta
    std::map<int, EnvFieldMeta> meta_by_index;
    for (const auto& m : g_env_fields) {
        meta_by_index[m.index] = m;
    }

    for (size_t i = 0; i < all_arrays.size(); ++i) {
        if (i >= partitions.size()) continue;
        const auto& p = partitions[i];
        const auto& arrays = all_arrays[i];

        for (const auto& arr : arrays) {
            int field_index = static_cast<int>(arr.field_index());
            auto it = meta_by_index.find(field_index);
            if (it == meta_by_index.end()) continue;
            const auto& meta = it->second;

            // For now, handle FIXED_ARRAY of int32 (matches test1.cpp c[5])
            if (meta.kind != "FIXED_ARRAY" ||
                meta.elem_size != 4 ||
                meta.fixed_length <= 0)
                continue;

            if (meta.offset + sizeof(uintptr_t) > original_env_size) continue;

            // Destination pointer in *master* env (safe to dereference)
            uintptr_t dst_ptr_val = 0;
            memcpy(&dst_ptr_val,
                   static_cast<char*>(original_env) + meta.offset,
                   sizeof(uintptr_t));
            if (!dst_ptr_val) continue;
            auto* dst = reinterpret_cast<int32_t*>(dst_ptr_val);

            // Source data from worker array payload (pure bytes, no pointer dereferencing)
            const int32_t* src = reinterpret_cast<const int32_t*>(arr.data().data());
            uint64_t len = arr.length();

            int64_t start = p.start;
            int64_t end   = p.end;
            int64_t step  = p.step;

            // Copy only indices this partition was responsible for
            for (int64_t idx = start; idx < end; idx += step) {
                if (idx < 0 || static_cast<uint64_t>(idx) >= len) continue;
                if (static_cast<int64_t>(idx) >= meta.fixed_length) continue;
                dst[idx] = src[idx];
            }
        }
    }

    // Debug: show merged env on master after applying all partitions
    debug_dump_env("master merged env (from worker arrays)", original_env, original_env_size);
    
    // Additional prominent debug: print merged FIXED_ARRAY results
    std::cerr << "\n========================================\n";
    std::cerr << "[LATTICE MERGE RESULT]\n";
    for (const auto& meta : g_env_fields) {
        if (meta.kind == "FIXED_ARRAY" && meta.elem_size == 4 && meta.fixed_length > 0) {
            if (meta.offset + sizeof(uintptr_t) > original_env_size) continue;
            
            uintptr_t ptr_val = 0;
            memcpy(&ptr_val, static_cast<char*>(original_env) + meta.offset, sizeof(uintptr_t));
            if (!ptr_val) continue;
            
            const int32_t* arr = reinterpret_cast<const int32_t*>(ptr_val);
            uint64_t len = static_cast<uint64_t>(meta.fixed_length);
            
            std::cerr << "  Merged array (field " << meta.index << ", len=" << len << "): [";
            for (uint64_t i = 0; i < len; ++i) {
                std::cerr << arr[i];
                if (i + 1 < len) std::cerr << ", ";
            }
            std::cerr << "]\n";
        }
    }
    std::cerr << "========================================\n\n";
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
    std::vector<std::vector<task::EnvArrayField>> all_arrays(partitions.size());
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
                &result_env, &result_size,
                &all_arrays[i]  // Capture arrays from worker
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
    
    // Merge results using arrays (safe: no pointer dereferencing)
    merge_results(all_arrays, partitions, env, g_current_env_size);
    
    // Free temporary result buffers
    for (void* env_ptr : result_envs) {
        if (env_ptr) free(env_ptr);
    }
    
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