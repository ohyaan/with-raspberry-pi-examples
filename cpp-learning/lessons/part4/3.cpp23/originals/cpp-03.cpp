#include <expected>
#include <string>

enum class FileError {
    NotFound,
    AccessDenied
};

// Returns either the file content (std::string) or a FileError enum
std::expected<std::string, FileError> read_device_config(int device_id) {
    if (device_id < 0) {
        return std::unexpected(FileError::NotFound); // Wrap error
    }
    return "BaudRate=115200\nDataBits=8"; // Success path
}

int main() {
    auto result = read_device_config(-1);
    
    if (result.has_value()) {
        // Success
        std::string config = result.value();
    } else {
        if (result.error() == FileError::NotFound) {
            // Handle error
        }
    }
    
    return 0;
}
