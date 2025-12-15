#include <iostream>
#include <string>
#include <curl/curl.h>
#include <json/json.h>
#include <sstream>

// Callback function for curl
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append((char*)contents, totalSize);
    return totalSize;
}

bool registerWorkerWithMaster(const std::string& master_url, 
                              const std::string& worker_address) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
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
            std::cout << "Successfully registered with master: " << response["message"].asString() << std::endl;
            return true;
        } else {
            std::cerr << "Registration error: " 
                      << (response.isMember("message") ? response["message"].asString() : "Unknown error") 
                      << std::endl;
        }
    }
    
    return false;
}

// Simple version without curl (using system curl command)
bool registerWorkerSimple(const std::string& master_url, 
                         const std::string& worker_address) {
    std::stringstream cmd;
    cmd << "curl -X POST " << master_url 
        << " -H 'Content-Type: application/json' "
        << " -d '{\"address\":\"" << worker_address << "\"}'";
    
    int result = system(cmd.str().c_str());
    return (result == 0);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <master_url> <worker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " http://192.168.1.50:8080/register 192.168.1.100:50051" << std::endl;
        return 1;
    }
    
    std::string master_url = argv[1];
    std::string worker_address = argv[2];
    
    // Try with curl library first, fallback to system curl
    if (registerWorkerWithMaster(master_url, worker_address) || 
        registerWorkerSimple(master_url, worker_address)) {
        std::cout << "Worker registered successfully!" << std::endl;
        return 0;
    }
    
    std::cerr << "Failed to register worker" << std::endl;
    return 1;
}