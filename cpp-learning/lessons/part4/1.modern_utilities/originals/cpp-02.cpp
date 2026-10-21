#include <variant>
#include <string>
#include <iostream>

// Define a variant that can hold an int, a double, or a string
using Payload = std::variant<int, double, std::string>;

int main() {
    Payload data;
    
    data = 42;             // Currently holds int
    data = "Pi in sky";    // Transferred to std::string
    
    // 1. Safe Type Queries
    if (std::holds_alternative<std::string>(data)) {
        std::cout << "String: " << std::get<std::string>(data) << "\n";
    }
    
    // 2. Safe Pointer Queries
    if (auto pVal = std::get_if<int>(&data)) {
        std::cout << "Int: " << *pVal << "\n"; // Safely returns nullptr if type mismatch
    }
    
    return 0;
}
