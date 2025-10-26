// main.cpp
#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <drogon/HttpAppFramework.h>
#include <json/json.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <string>

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
                    std::string cmd = "clang++ -S -emit-llvm -o" + savedPath + ".ll " + savedPath;
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

                    std::string loop_outline_pass =
                        "opt -load-pass-plugin /home/kazisahib/dcs/Master/libLoopOutliner.so "
                        "-passes='loop-outliner' -S " +
                        savedPath + ".opt.ll -o " + savedPath + ".opt.ll";

                    int ret4 = std::system(loop_outline_pass.c_str());
                    if (ret4 != 0)
                    {
                        throw std::runtime_error("Failed to outline loops");
                    }
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
