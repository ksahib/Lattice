#include "worker_manager.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <json/json.h>

WorkerManager::WorkerManager(const std::string& file_path) 
    : storage_file_(file_path), last_used_index_(0) {
    loadFromFile();
}

void WorkerManager::saveToFileUnlocked() {
    // Internal version - assumes mutex is already locked
    Json::Value root;
    Json::Value workers_array(Json::arrayValue);
    
    for (const auto& worker : workers_) {
        Json::Value worker_obj;
        worker_obj["address"] = worker.address;
        worker_obj["status"] = (worker.status == WorkerStatus::AVAILABLE) ? "available" : "busy";
        workers_array.append(worker_obj);
    }
    
    root["workers"] = workers_array;
    
    std::ofstream file(storage_file_);
    if (file.is_open()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(root, &file);
        file.close();
    }
}

void WorkerManager::saveToFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    saveToFileUnlocked();
}

void WorkerManager::loadFromFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    workers_.clear();
    
    std::ifstream file(storage_file_);
    if (!file.is_open()) {
        // File doesn't exist yet, that's okay
        return;
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    if (Json::parseFromStream(builder, file, &root, &errors)) {
        if (root.isMember("workers") && root["workers"].isArray()) {
            for (const auto& worker_obj : root["workers"]) {
                if (worker_obj.isMember("address")) {
                    std::string address = worker_obj["address"].asString();
                    std::string status_str = "available";
                    if (worker_obj.isMember("status")) {
                        status_str = worker_obj["status"].asString();
                    }
                    WorkerStatus status = (status_str == "available") ? 
                                         WorkerStatus::AVAILABLE : WorkerStatus::BUSY;
                    workers_.emplace_back(address, status);
                }
            }
        }
    }
    file.close();
}

bool WorkerManager::registerWorker(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if worker already exists
    auto it = std::find_if(workers_.begin(), workers_.end(),
        [&address](const WorkerInfo& w) { return w.address == address; });
    
    if (it != workers_.end()) {
        // Worker exists, just mark as available
        it->status = WorkerStatus::AVAILABLE;
    } else {
        // New worker, add it
        workers_.emplace_back(address, WorkerStatus::AVAILABLE);
    }
    
    saveToFileUnlocked();
    std::cout << "Registered worker: " << address << std::endl;
    return true;
}

bool WorkerManager::unregisterWorker(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::remove_if(workers_.begin(), workers_.end(),
        [&address](const WorkerInfo& w) { return w.address == address; });
    
    if (it != workers_.end()) {
        workers_.erase(it, workers_.end());
        saveToFileUnlocked();
        std::cout << "Unregistered worker: " << address << std::endl;
        return true;
    }
    
    return false;
}

std::vector<WorkerInfo> WorkerManager::getAllWorkers() {
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_;
}

std::vector<WorkerInfo> WorkerManager::getAvailableWorkers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WorkerInfo> available;
    
    for (const auto& worker : workers_) {
        if (worker.status == WorkerStatus::AVAILABLE) {
            available.push_back(worker);
        }
    }
    
    return available;
}

bool WorkerManager::setWorkerStatus(const std::string& address, WorkerStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(workers_.begin(), workers_.end(),
        [&address](const WorkerInfo& w) { return w.address == address; });
    
    if (it != workers_.end()) {
        it->status = status;
        saveToFileUnlocked();
        return true;
    }
    
    return false;
}

std::string WorkerManager::getNextAvailableWorker() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get available workers
    std::vector<WorkerInfo> available;
    for (const auto& worker : workers_) {
        if (worker.status == WorkerStatus::AVAILABLE) {
            available.push_back(worker);
        }
    }
    
    if (available.empty()) {
        return "";  // No available workers
    }
    
    // Round-robin: select next available worker
    std::string selected = available[last_used_index_ % available.size()].address;
    last_used_index_++;
    
    return selected;
}

size_t WorkerManager::getWorkerCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_.size();
}

size_t WorkerManager::getAvailableWorkerCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& worker : workers_) {
        if (worker.status == WorkerStatus::AVAILABLE) {
            count++;
        }
    }
    return count;
}

