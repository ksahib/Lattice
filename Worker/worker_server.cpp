#include <grpcpp/grpcpp.h>
#include "task_service.grpc.pb.h"
#include <iostream>
#include <dlfcn.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <curl/curl.h>
#include <json/json.h>
#include <fstream>
#include <map>
#include <mutex>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using task::TaskService;
using task::TaskRequest;
using task::TaskResponse;

// Function pointer type for outlined wrapper:
//   wrapper(i64 idx, i8* env) -> void
using outlined_fn_t = void (*)(int64_t, void*);

// Env metadata structures
struct EnvFieldMeta {
    int index = -1;
    uint64_t offset = 0;
    std::string kind; // "SCALAR" or "POINTER_ARRAY"
    uint64_t elem_size = 0;
    int len_field = -1;
};

static std::vector<EnvFieldMeta> g_env_fields;
static uint64_t g_env_struct_size = 0;
static std::once_flag g_env_meta_once;

static void load_env_metadata() {
    std::call_once(g_env_meta_once, []() {
        // Allow overriding via ENV_METADATA_PATH env var, else use local Worker path, else /tmp.
        const char* env_override = std::getenv("ENV_METADATA_PATH");
        const char* fallback1 = "/home/niloy/vs_code/course/cse299/Lattice/Worker/env_metadata.json";
        const char* fallback2 = "/tmp/env_metadata.json";
        const char* path = env_override ? env_override : fallback1;
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            // try fallback
            path = fallback2;
            ifs.open(path);
        }
        if (!ifs.is_open()) {
            std::cerr << "env meta: cannot open " << path << " (also tried env override and /tmp)\n";
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
                g_env_fields.push_back(m);
            }
        }
    });
}

class TaskServiceImpl final : public TaskService::Service {
    Status ExecuteTask(ServerContext* context,
                      const TaskRequest* request,
                      TaskResponse* response) override {
        
        response->set_task_id(request->task_id());
        
        try {
            // Deserialize environment
            size_t env_size = request->environment_size();
            void* env = malloc(env_size);
            if (!env) {
                response->set_success(false);
                response->set_error_message("Failed to allocate environment");
                return Status::OK;
            }
            
            memcpy(env, request->environment_data().data(), 
                   std::min(env_size, request->environment_data().size()));

            // Reconstruct pointer arrays using metadata and request.arrays
            load_env_metadata();
            std::map<int, const task::EnvArrayField*> arrayMap;
            for (int i = 0; i < request->arrays_size(); ++i) {
                const task::EnvArrayField& a = request->arrays(i);
                arrayMap[a.field_index()] = &a;
            }
            for (const auto& meta : g_env_fields) {
                if (meta.kind != "POINTER_ARRAY") continue;
                auto it = arrayMap.find(meta.index);
                if (it == arrayMap.end()) {
                    continue;
                }
                const task::EnvArrayField* arr = it->second;
                uint64_t len = arr->length();
                uint64_t bytes = len * meta.elem_size;
                if (bytes == 0 || arr->data().size() < bytes) continue;
                void* buf = malloc(bytes);
                if (!buf) continue;
                memcpy(buf, arr->data().data(), bytes);
                // write pointer value into env at offset
                if (meta.offset + sizeof(uintptr_t) <= env_size) {
                    uintptr_t p = reinterpret_cast<uintptr_t>(buf);
                    memcpy(static_cast<char*>(env) + meta.offset, &p, sizeof(uintptr_t));
                }
            }
            
            // --- Compile IR on worker and load outlined function ---
            // 1) Write IR to a temporary file
            std::string ir_path = "/tmp/task_" + std::to_string(request->task_id()) + ".ll";
            {
                std::ofstream ofs(ir_path, std::ios::binary);
                ofs.write(request->ir_data().data(), request->ir_data().size());
            }

            // 2) Compile IR to a shared object
            std::string so_path = "/tmp/libtask_" + std::to_string(request->task_id()) + ".so";
            {
                std::stringstream cmd;
                cmd << "clang++-20 -shared -fPIC -O2 "
                    << ir_path << " -o " << so_path;
                int compile_ret = system(cmd.str().c_str());
                if (compile_ret != 0) {
                    free(env);
                    response->set_success(false);
                    response->set_error_message("Failed to compile IR on worker");
                    return Status::OK;
                }
            }

            // 3) dlopen the shared object
            void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
            if (!handle) {
                free(env);
                response->set_success(false);
                response->set_error_message(std::string("dlopen failed: ") + dlerror());
                return Status::OK;
            }

            // 4) dlsym the outlined function
            outlined_fn_t outlined_fn =
                (outlined_fn_t)dlsym(handle, request->function_name().c_str());
            if (!outlined_fn) {
                dlclose(handle);
                free(env);
                response->set_success(false);
                response->set_error_message("Failed to load outlined function");
                return Status::OK;
            }

            // Execute loop iterations via wrapper(idx, env)
            int64_t start = request->start();
            int64_t end = request->end();
            int64_t step = request->step();
            
            for (int64_t i = start; i < end; i += step) {
                outlined_fn(i, env);
            }
            
            // Serialize updated environment as result
            response->set_result_data(std::string(static_cast<char*>(env), env_size));
            response->set_result_size(env_size);
            response->set_success(true);
            
            dlclose(handle);
            free(env);
            return Status::OK;
            
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_error_message(std::string("Exception: ") + e.what());
            return Status::OK;
        }
    }
};

void RunWorkerServer(const std::string& server_address) {
    TaskServiceImpl service;
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (!server) {
        std::cerr << "Failed to start worker server on " << server_address << std::endl;
        return;
    }
    
    std::cout << "Worker server listening on " << server_address << std::endl;
    server->Wait();
}

// Callback function for curl
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append((char*)contents, totalSize);
    return totalSize;
}

// Register worker with master server
bool registerWorkerWithMaster(const std::string& master_url, 
                              const std::string& worker_address) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl, trying system curl..." << std::endl;
        return false;
    }
    
    // Prepare JSON payload
    Json::Value request;
    request["address"] = worker_address;
    
    Json::StreamWriterBuilder builder;
    std::string json_payload = Json::writeString(builder, request);
    
    std::string response_data;
    
    // Setup curl
    curl_easy_setopt(curl, CURLOPT_URL, master_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || response_code != 200) {
        std::cerr << "Registration failed: " << curl_easy_strerror(res) 
                  << " (HTTP " << response_code << ")" << std::endl;
        return false;
    }
    
    // Parse response
    Json::Value response;
    Json::Reader reader;
    if (reader.parse(response_data, response)) {
        if (response.isMember("result") && response["result"].asString() == "ok") {
            std::cout << "✓ Successfully registered with master: " 
                      << response["message"].asString() << std::endl;
            return true;
        } else {
            std::cerr << "Registration error: " 
                      << (response.isMember("message") ? response["message"].asString() : "Unknown error") 
                      << std::endl;
        }
    }
    
    return false;
}

// Fallback: Use system curl command
bool registerWorkerSimple(const std::string& master_url, 
                         const std::string& worker_address) {
    std::stringstream cmd;
    cmd << "curl -X POST " << master_url 
        << " -H 'Content-Type: application/json' "
        << " -d '{\"address\":\"" << worker_address << "\"}' 2>/dev/null";
    
    int result = system(cmd.str().c_str());
    return (result == 0);
}

void printUsage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " --master=<master_url> --ip=<worker_ip> [--port=<port>]" << std::endl;
    std::cerr << "   or: " << prog_name << " <master_url> <worker_ip> [port]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  " << prog_name << " --master=http://192.168.1.50:8080 --ip=192.168.1.100" << std::endl;
    std::cerr << "  " << prog_name << " --master=http://192.168.1.50:8080 --ip=192.168.1.100 --port=50051" << std::endl;
    std::cerr << "  " << prog_name << " http://192.168.1.50:8080 192.168.1.100 50051" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Arguments:" << std::endl;
    std::cerr << "  --master, -m    Master server URL (e.g., http://192.168.1.50:8080)" << std::endl;
    std::cerr << "  --ip, -i        Worker IP address (e.g., 192.168.1.100)" << std::endl;
    std::cerr << "  --port, -p      Worker port (default: 50051)" << std::endl;
}

int main(int argc, char** argv) {
    std::string master_url;
    std::string worker_ip;
    int worker_port = 50051;  // Fixed default port
    
    // Parse command-line arguments
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    // Check if using --option format or positional
    bool use_options = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("--") == 0 || arg.find("-") == 0) {
            use_options = true;
            break;
        }
    }
    
    if (use_options) {
        // Parse --option format
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--master" || arg == "-m") {
                if (i + 1 < argc) {
                    master_url = argv[++i];
                } else {
                    std::cerr << "Error: --master requires a URL" << std::endl;
                    return 1;
                }
            } else if (arg == "--ip" || arg == "-i") {
                if (i + 1 < argc) {
                    worker_ip = argv[++i];
                } else {
                    std::cerr << "Error: --ip requires an IP address" << std::endl;
                    return 1;
                }
            } else if (arg == "--port" || arg == "-p") {
                if (i + 1 < argc) {
                    worker_port = std::stoi(argv[++i]);
                } else {
                    std::cerr << "Error: --port requires a port number" << std::endl;
                    return 1;
                }
            } else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            } else if (arg.find("--master=") == 0) {
                master_url = arg.substr(9);
            } else if (arg.find("--ip=") == 0) {
                worker_ip = arg.substr(5);
            } else if (arg.find("--port=") == 0) {
                worker_port = std::stoi(arg.substr(7));
            }
        }
    } else {
        // Parse positional arguments: <master_url> <worker_ip> [port]
        master_url = argv[1];
        worker_ip = argv[2];
        if (argc > 3) {
            worker_port = std::stoi(argv[3]);
        }
    }
    
    // Validate arguments
    if (master_url.empty()) {
        std::cerr << "Error: Master URL is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    if (worker_ip.empty()) {
        std::cerr << "Error: Worker IP address is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Ensure master_url has http:// prefix
    if (master_url.find("http://") != 0 && master_url.find("https://") != 0) {
        master_url = "http://" + master_url;
    }
    
    // Build registration URL and worker address
    std::string register_url = master_url;
    if (register_url.back() != '/') {
        register_url += "/";
    }
    register_url += "register";
    
    std::string worker_address = worker_ip + ":" + std::to_string(worker_port);
    std::string grpc_server_address = "0.0.0.0:" + std::to_string(worker_port);
    
    std::cout << "===========================================" << std::endl;
    std::cout << "Worker Server Starting..." << std::endl;
    std::cout << "  Master URL: " << master_url << std::endl;
    std::cout << "  Worker Address: " << worker_address << std::endl;
    std::cout << "  Port: " << worker_port << std::endl;
    std::cout << "===========================================" << std::endl;
    
    // Auto-register with master
    std::cout << "\nRegistering with master server..." << std::endl;
    bool registered = registerWorkerWithMaster(register_url, worker_address);
    
    if (!registered) {
        std::cout << "Trying fallback registration method..." << std::endl;
        registered = registerWorkerSimple(register_url, worker_address);
    }
    
    if (!registered) {
        std::cerr << "Warning: Failed to register with master server." << std::endl;
        std::cerr << "The worker will still start, but may not receive tasks." << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "  1. Master server is running on " << master_url << std::endl;
        std::cerr << "  2. Network connectivity" << std::endl;
        std::cerr << "  3. Firewall settings" << std::endl;
    }
    
    std::cout << "\nStarting gRPC server..." << std::endl;
    
    // Start the gRPC server
    RunWorkerServer(grpc_server_address);
    
    return 0;
}