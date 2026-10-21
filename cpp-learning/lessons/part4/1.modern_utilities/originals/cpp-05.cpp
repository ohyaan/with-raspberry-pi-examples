#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    fs::path log_dir = "/tmp/var/log/my_app";
    
    // 1. Directory Checks & Creation
    if (!fs::exists(log_dir)) {
        fs::create_directories(log_dir); // Creates recursive paths if needed
    }
    
    // 2. Inspecting Files & Metadata
    fs::path config_file = "config.txt";
    if (fs::exists(config_file)) {
        std::cout << "File size: " << fs::file_size(config_file) << " bytes\n";
    }
    
    // 3. Iterating Directories
    fs::path current_dir = fs::current_path();
    std::cout << "Files in " << current_dir << ":\n";
    for (const auto& entry : fs::directory_iterator(current_dir)) {
        std::cout << "- " << entry.path().filename().string() 
                  << " [" << (entry.is_directory() ? "DIR" : "FILE") << "]\n";
    }
    
    return 0;
}
