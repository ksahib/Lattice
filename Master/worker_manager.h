#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>

enum class WorkerStatus {
    AVAILABLE,
    BUSY
};

struct WorkerInfo {
    std::string address;  // Format: "IP:PORT" (e.g., "192.168.1.100:50051")
    WorkerStatus status;
    
    WorkerInfo(const std::string& addr, WorkerStatus st = WorkerStatus::AVAILABLE) 
        : address(addr), status(st) {}
};

class WorkerManager {
private:
    std::vector<WorkerInfo> workers_;
    std::mutex mutex_;
    std::string storage_file_;  // Path to workers.json
    size_t last_used_index_;    // For round-robin
    
    void saveToFileUnlocked();  // Internal version without lock
    void saveToFile();          // Public version with lock
    void loadFromFile();
    
public:
    WorkerManager(const std::string& file_path = "./workers.json");
    
    // Register a new worker
    bool registerWorker(const std::string& address);
    
    // Unregister a worker
    bool unregisterWorker(const std::string& address);
    
    // Get all workers
    std::vector<WorkerInfo> getAllWorkers();
    
    // Get available workers only
    std::vector<WorkerInfo> getAvailableWorkers();
    
    // Set worker status
    bool setWorkerStatus(const std::string& address, WorkerStatus status);
    
    // Round-robin: Get next available worker
    std::string getNextAvailableWorker();
    
    // Get worker count
    size_t getWorkerCount();
    size_t getAvailableWorkerCount();
    
};

#endif // WORKER_MANAGER_H