// main.cpp
#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <drogon/HttpAppFramework.h>
#include <json/json.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <exception>

#include "worker_manager.h"

using namespace drogon;

// Helper function to add CORS headers
void addCorsHeaders(const HttpResponsePtr &resp)
{
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
}
WorkerManager workerManager("./workers.json");

int main()
{
    // Basic server setup (chained calls; no semicolon until the end)
    drogon::app()
        .setLogLevel(trantor::Logger::kInfo)
        .setUploadPath("./uploads")
        .setClientMaxBodySize(200 * 1024 * 1024)
        .addListener("0.0.0.0", 8080);

    std::filesystem::create_directories("./uploads");

    std::cout << "Server starting on port 8080..." << std::endl;
    
    drogon::app().registerHandler(
        "/register",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            Json::Value response;
            
            try {
                // Handle OPTIONS preflight
                if (req->getMethod() == drogon::Options)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k200OK);
                    callback(resp);
                    return;
                }
                
                if (req->getMethod() != drogon::Post)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k405MethodNotAllowed);
                    resp->setBody("Use POST method");
                    callback(resp);
                    return;
                }
                
                Json::Reader reader;
                Json::Value request_body;
                
                // Parse JSON body
                std::string body = std::string(req->getBody());
                if (!reader.parse(body, request_body))
                {
                    response["result"] = "error";
                    response["message"] = "Invalid JSON";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                // Extract IP and port
                if (!request_body.isMember("address"))
                {
                    response["result"] = "error";
                    response["message"] = "Missing 'address' field (format: 'IP:PORT')";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                std::string address = request_body["address"].asString();
                
                // Validate format (simple check)
                if (address.find(':') == std::string::npos)
                {
                    response["result"] = "error";
                    response["message"] = "Invalid address format. Use 'IP:PORT' (e.g., '192.168.1.100:50051')";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                // Register worker
                bool success = workerManager.registerWorker(address);
                
                if (success)
                {
                    response["result"] = "ok";
                    response["message"] = "Worker registered successfully";
                    response["address"] = address;
                    response["total_workers"] = static_cast<int>(workerManager.getWorkerCount());
                    response["available_workers"] = static_cast<int>(workerManager.getAvailableWorkerCount());
                }
                else
                {
                    response["result"] = "error";
                    response["message"] = "Failed to register worker";
                }
                
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
                
            } catch (const std::exception& e) {
                response["result"] = "error";
                response["message"] = std::string("Exception: ") + e.what();
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
            }
        },
        {Post, Options});


    drogon::app().registerHandler(
        "/workers",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            Json::Value response;
            Json::Value workers_array(Json::arrayValue);
            
            auto all_workers = workerManager.getAllWorkers();
            for (const auto& worker : all_workers)
            {
                Json::Value worker_obj;
                worker_obj["address"] = worker.address;
                worker_obj["status"] = (worker.status == WorkerStatus::AVAILABLE) ? "available" : "busy";
                workers_array.append(worker_obj);
            }
            
            response["result"] = "ok";
            response["workers"] = workers_array;
            response["total"] = static_cast<int>(workerManager.getWorkerCount());
            response["available"] = static_cast<int>(workerManager.getAvailableWorkerCount());
            
            auto resp = HttpResponse::newHttpJsonResponse(response);
            addCorsHeaders(resp);
            callback(resp);
        },
        {Get, Options});   

    // Unregister Worker Endpoint
    drogon::app().registerHandler(
        "/unregister",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            Json::Value response;
            
            try {
                // Handle OPTIONS preflight
                if (req->getMethod() == drogon::Options)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k200OK);
                    callback(resp);
                    return;
                }
                
                if (req->getMethod() != drogon::Post)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k405MethodNotAllowed);
                    resp->setBody("Use POST method");
                    callback(resp);
                    return;
                }
                
                Json::Reader reader;
                Json::Value request_body;
                
                // Parse JSON body
                std::string body = std::string(req->getBody());
                if (!reader.parse(body, request_body))
                {
                    response["result"] = "error";
                    response["message"] = "Invalid JSON";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                // Extract address
                if (!request_body.isMember("address"))
                {
                    response["result"] = "error";
                    response["message"] = "Missing 'address' field";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                std::string address = request_body["address"].asString();
                
                // Unregister worker
                bool success = workerManager.unregisterWorker(address);
                
                if (success)
                {
                    response["result"] = "ok";
                    response["message"] = "Worker unregistered successfully";
                    response["address"] = address;
                    response["total_workers"] = static_cast<int>(workerManager.getWorkerCount());
                }
                else
                {
                    response["result"] = "error";
                    response["message"] = "Worker not found or failed to unregister";
                }
                
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
                
            } catch (const std::exception& e) {
                response["result"] = "error";
                response["message"] = std::string("Exception: ") + e.what();
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
            }
        },
        {Post, Options});

    // Set Worker Status Endpoint
    drogon::app().registerHandler(
        "/status",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            Json::Value response;
            
            try {
                // Handle OPTIONS preflight
                if (req->getMethod() == drogon::Options)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k200OK);
                    callback(resp);
                    return;
                }
                
                if (req->getMethod() != drogon::Post)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k405MethodNotAllowed);
                    resp->setBody("Use POST method");
                    callback(resp);
                    return;
                }
                
                Json::Reader reader;
                Json::Value request_body;
                
                // Parse JSON body
                std::string body = std::string(req->getBody());
                if (!reader.parse(body, request_body))
                {
                    response["result"] = "error";
                    response["message"] = "Invalid JSON";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                // Extract address and status
                if (!request_body.isMember("address") || !request_body.isMember("status"))
                {
                    response["result"] = "error";
                    response["message"] = "Missing 'address' or 'status' field";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                std::string address = request_body["address"].asString();
                std::string status_str = request_body["status"].asString();
                
                // Validate status
                WorkerStatus status;
                if (status_str == "available" || status_str == "AVAILABLE")
                {
                    status = WorkerStatus::AVAILABLE;
                }
                else if (status_str == "busy" || status_str == "BUSY")
                {
                    status = WorkerStatus::BUSY;
                }
                else
                {
                    response["result"] = "error";
                    response["message"] = "Invalid status. Use 'available' or 'busy'";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    addCorsHeaders(resp);
                    callback(resp);
                    return;
                }
                
                // Set worker status
                bool success = workerManager.setWorkerStatus(address, status);
                
                if (success)
                {
                    response["result"] = "ok";
                    response["message"] = "Worker status updated successfully";
                    response["address"] = address;
                    response["status"] = status_str;
                }
                else
                {
                    response["result"] = "error";
                    response["message"] = "Worker not found";
                }
                
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
                
            } catch (const std::exception& e) {
                response["result"] = "error";
                response["message"] = std::string("Exception: ") + e.what();
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
            }
        },
        {Post, Options});

    // Get Next Available Worker Endpoint (for scheduling)
    drogon::app().registerHandler(
        "/next-worker",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            Json::Value response;
            
            try {
                if (req->getMethod() == drogon::Options)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k200OK);
                    callback(resp);
                    return;
                }
                
                if (req->getMethod() != drogon::Get)
                {
                    auto resp = HttpResponse::newHttpResponse();
                    addCorsHeaders(resp);
                    resp->setStatusCode(k405MethodNotAllowed);
                    resp->setBody("Use GET method");
                    callback(resp);
                    return;
                }
                
                // Get next available worker (round-robin)
                std::string next_worker = workerManager.getNextAvailableWorker();
                
                if (!next_worker.empty())
                {
                    response["result"] = "ok";
                    response["address"] = next_worker;
                    response["message"] = "Next available worker selected";
                }
                else
                {
                    response["result"] = "error";
                    response["message"] = "No available workers";
                }
                
                response["total_workers"] = static_cast<int>(workerManager.getWorkerCount());
                response["available_workers"] = static_cast<int>(workerManager.getAvailableWorkerCount());
                
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
                
            } catch (const std::exception& e) {
                response["result"] = "error";
                response["message"] = std::string("Exception: ") + e.what();
                auto resp = HttpResponse::newHttpJsonResponse(response);
                addCorsHeaders(resp);
                callback(resp);
            }
        },
        {Get, Options});

    // Handle OPTIONS preflight requests
    drogon::app().registerHandler(
        "/upload",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            if (req->getMethod() == drogon::Options)
            {
                auto resp = HttpResponse::newHttpResponse();
                addCorsHeaders(resp);
                resp->setStatusCode(k200OK);
                callback(resp);
                return;
            }
            
            if (req->getMethod() != drogon::Post)
            {
                auto resp = HttpResponse::newHttpResponse();
                addCorsHeaders(resp);
                resp->setStatusCode(k405MethodNotAllowed);
                resp->setBody("Use POST multipart/form-data");
                callback(resp);
                return;
            }

            MultiPartParser parser;
            if (parser.parse(req) != 0)
            {
                Json::Value j;
                j["result"] = "parse_error";
                j["message"] = "Failed to parse multipart/form-data";
                auto resp = HttpResponse::newHttpJsonResponse(j);
                addCorsHeaders(resp);
                callback(resp);
                return;
            }

            auto files = parser.getFiles();
            Json::Value out;
            out["result"] = "ok";
            out["files_received"] = static_cast<int>(files.size());
            Json::Value arr(Json::arrayValue);

            for (const auto &f : files)
            {
                std::string savedName = f.getFileName();
                std::string savedPath = "./uploads/" + savedName;
                // clang++ -S -emit-llvm -o
                try
                {
                    f.saveAs(savedPath);
                    std::string cmd = "clang++-20 -O1 -S -emit-llvm -o " + savedPath + ".ll " + savedPath;
                    int ret = std::system(cmd.c_str());
                    if (ret != 0)
                    {
                        throw std::runtime_error("Failed to generate LLVM IR");
                    }
                    std::string llvm_passes = "opt-20 \
    -passes='mem2reg,loop-simplify,lcssa,simplifycfg' \
    -S " + savedPath +
                                              ".ll " + "-o " + savedPath + ".opt.ll";
                    int ret2 = std::system(llvm_passes.c_str());
                    if (ret2 != 0)
                    {
                        throw std::runtime_error("Failed to optimize LLVM IR");
                    }
                    std::string pdg_pass = "opt-20 -load-pass-plugin /home/niloy/vs_code/course/cse299/Lattice/Master/libPDGPass.so \
    -passes='pdg-builder' \
    -S " + savedPath + ".opt.ll " + "-o " + savedPath +
                                           ".opt.ll";

                    int ret3 = std::system(pdg_pass.c_str());
                    if (ret3 != 0)
                    {
                        throw std::runtime_error("Failed to generate PDG");
                    }

                    const std::string OPT = "/usr/bin/opt-20"; // set to `which opt` if different
                    const std::string PLUGIN = "/home/niloy/vs_code/course/cse299/Lattice/Master/libLoopOutliner.so";
                    std::string loop_outline_pass =
                        OPT + " -load-pass-plugin=" + PLUGIN +
                        " -passes='function(loop-outliner)' -S " + savedPath + ".opt.ll -o " + savedPath + ".opt.ll"
                                                                                                           " 2>&1 | tee /tmp/opt_run.log";

                    int ret4 = std::system(loop_outline_pass.c_str());
                    if (ret4 != 0)
                    {
                        throw std::runtime_error("Failed to outline loops");
                    }

                    // Link final program using gRPC-based parallel runtime.
                    // We delegate to a shell script that knows how to find vcpkg + gRPC libs.
                    // NOTE: Adjust the path to link_with_grpc.sh if you move this project.
                    std::string final_pass =
                        "bash /home/niloy/vs_code/course/cse299/Lattice/Master/link_with_grpc.sh " +
                        savedPath + ".opt.ll";

                    int ret5 = std::system(final_pass.c_str());
                    if (ret5 != 0)
                    {
                        throw std::runtime_error("Failed to compile final program");
                    }

                    // Expose current IR path to the gRPC runtime so it can send IR to workers.
                    std::string ir_path = savedPath + ".opt.ll";
                    setenv("LATTICE_CURRENT_IR", ir_path.c_str(), 1);

                    std::string run_command = "./runprog";
                    int ret6 = std::system(run_command.c_str());
                    if (ret6 != 0)
                    {
                        throw std::runtime_error("Failed to run final program");
                    }

                    // Replace this section where you call system() for the loop outline pass
                    // {
                    //     const std::string OPT = "/usr/bin/opt";                                     // absolute path to opt
                    //     const std::string PLUGIN = "/home/kazisahib/dcs/Master/libLoopOutliner.so"; // absolute plugin path

                    //     // Set LD_LIBRARY_PATH to the directory where your LLVM shared libs live.
                    //     // Adjust if your llvm libs are under /usr/lib/llvm-14/lib or similar.
                    //     const std::string LLVM_LIB_DIR = "/usr/lib/llvm-14/lib";
             
                    //     // Log file
                    //     const std::string LOG = "/tmp/opt_run.log";

                    //     // Build the full shell command. Use -lc so shell handles quotes reliably.
                    //     std::string cmd = "env LD_LIBRARY_PATH=" + LLVM_LIB_DIR +
                    //                       " " + OPT +
                    //                       " -load-pass-plugin=" + PLUGIN +
                    //                       " -passes='function(loop-outliner)' -S " + savedPath + ".opt.ll -o " + savedPath + ".opt.ll"
                    //                                                                                                          " 2>&1 | tee " +
                    //                       LOG;

                    //     // Run the command
                    //     int sysret = std::system(cmd.c_str());
                    //     if (sysret == -1)
                    //     {
                    //         // system() failed to start the shell
                    //         std::string err = std::string("system() failed to run opt command: ") + std::strerror(errno);
                    //         throw std::runtime_error(err);
                    //     }

                    //     // Extract child exit status
                    //     if (WIFEXITED(sysret))
                    //     {
                    //         int exitCode = WEXITSTATUS(sysret);
                    //         // Print the opt log into server stderr so you see plugin output in server logs
                    //         std::ifstream ifs(LOG);
                    //         if (ifs)
                    //         {
                    //             std::string line;
                    //             std::cerr << "---- opt log start ----\n";
                    //             while (std::getline(ifs, line))
                    //                 std::cerr << line << "\n";
                    //             std::cerr << "---- opt log end (exit=" << exitCode << ") ----\n";
                    //         }
                    //         else
                    //         {
                    //             std::cerr << "Could not open opt log: " << LOG << "\n";
                    //         }

                    //         if (exitCode != 0)
                    //         {
                    //             throw std::runtime_error("opt returned non-zero exit code: " + std::to_string(exitCode));
                    //         }
                    //     }
                    //     else
                    //     {
                    //         throw std::runtime_error("opt did not exit normally");
                    //     }
                    // }
                }

                catch (const std::exception &e)
                {
                    Json::Value info;
                    info["original_name"] = f.getFileName();
                    info["status"] = "save_failed";
                    info["error"] = e.what();
                    arr.append(info);
                    continue;
                }

                Json::Value info;
                info["original_name"] = f.getFileName();
                info["saved_name"] = savedName;
                info["saved_path"] = savedPath;
                info["size"] = static_cast<Json::Int64>(f.fileLength());
                info["mime_type"] = f.getFileType();
                info["md5"] = f.getMd5();
                arr.append(info);
            }

            out["files"] = arr;
            for (const auto &p : parser.getParameters())
            {
                out["form_params"][p.first] = p.second;
            }

            auto resp = HttpResponse::newHttpJsonResponse(out);
            addCorsHeaders(resp);
            callback(resp);
        },
        {Post, Options});

    drogon::app().run();
    return 0;
}
