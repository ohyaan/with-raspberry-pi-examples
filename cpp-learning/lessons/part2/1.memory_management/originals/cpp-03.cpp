#include <fstream>
#include <mutex>

void write_to_file() {
    std::mutex file_mutex;
    
    // RAII Lock: locks mutex on initialization, unlocks automatically at end of scope
    std::lock_guard<std::mutex> lock(file_mutex); 
    
    // RAII File: opens file on initialization, closes automatically in destructor
    std::ofstream file("log.txt"); 
    file << "Writing secure log data.\n";
} // 'file' and 'lock' go out of scope here. Mutex unlocked, file closed.
