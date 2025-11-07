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

using namespace drogon;

int main()
{
    // Basic server setup (chained calls; no semicolon until the end)
    drogon::app()
        .setLogLevel(trantor::Logger::kInfo)
        .setUploadPath("./uploads")
        .setClientMaxBodySize(200 * 1024 * 1024)
        .addListener("0.0.0.0", 8080);

    std::filesystem::create_directories("./uploads");

    drogon::app().registerHandler(
        "/upload",
        [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
        {
            if (req->getMethod() != drogon::Post)
            {
                auto resp = HttpResponse::newHttpResponse();
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
                callback(HttpResponse::newHttpJsonResponse(j));
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
                    std::string cmd = "clang++ -O1 -S -emit-llvm -o " + savedPath + ".ll " + savedPath;
                    int ret = std::system(cmd.c_str());
                    if (ret != 0)
                    {
                        throw std::runtime_error("Failed to generate LLVM IR");
                    }
                    std::string llvm_passes = "opt \
    -passes='mem2reg,loop-simplify,lcssa,simplifycfg' \
    -S " + savedPath +
                                              ".ll " + "-o " + savedPath + ".opt.ll";
                    int ret2 = std::system(llvm_passes.c_str());
                    if (ret2 != 0)
                    {
                        throw std::runtime_error("Failed to optimize LLVM IR");
                    }
                    std::string pdg_pass = "opt -load-pass-plugin /home/kazisahib/dcs/Master/libPDGPass.so \
    -passes='pdg-builder' \
    -S " + savedPath + ".opt.ll " + "-o " + savedPath +
                                           ".opt.ll";

                    int ret3 = std::system(pdg_pass.c_str());
                    if (ret3 != 0)
                    {
                        throw std::runtime_error("Failed to generate PDG");
                    }

                    const std::string OPT = "/usr/bin/opt"; // set to `which opt` if different
                    const std::string PLUGIN = "/home/kazisahib/dcs/Master/libLoopOutliner.so";
                    std::string loop_outline_pass =
                        OPT + " -load-pass-plugin=" + PLUGIN +
                        " -passes='function(loop-outliner)' -S " + savedPath + ".opt.ll -o " + savedPath + ".opt.ll"
                                                                                                           " 2>&1 | tee /tmp/opt_run.log";

                    int ret4 = std::system(loop_outline_pass.c_str());
                    if (ret4 != 0)
                    {
                        throw std::runtime_error("Failed to outline loops");
                    }

                    std::string final_pass =
                        "clang++ -O2 " + savedPath + ".opt.ll /home/kazisahib/dcs/Master/parallel_runtime.o -o runprog -lpthread";

                    int ret5 = std::system(final_pass.c_str());
                    if (ret5 != 0)
                    {
                        throw std::runtime_error("Failed to compile final program");
                    }

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

            callback(HttpResponse::newHttpJsonResponse(out));
        },
        {Post});

    drogon::app().run();
    return 0;
}
