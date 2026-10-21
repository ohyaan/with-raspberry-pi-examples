#include <utility>
#include <cstdint>

enum class Status : uint8_t {
    Ok = 0,
    Error = 1
};

void run_status() {
    // Returns 1 as a uint8_t
    auto val = std::to_underlying(Status::Error); 
}
