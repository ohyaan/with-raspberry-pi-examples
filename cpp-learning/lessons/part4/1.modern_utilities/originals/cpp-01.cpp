#include <optional>
#include <string>
#include <iostream>

// Dummy helper
bool is_valid_number(const std::string& s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
}

// Returns an int if parsing is successful, otherwise returns std::nullopt
std::optional<int> parse_sensor_id(const std::string& input) {
    if (!is_valid_number(input)) {
        return std::nullopt; // Represents absence of value
    }
    try {
        return std::stoi(input); // Return actual int
    } catch (...) {
        return std::nullopt;
    }
}

int main() {
    auto id = parse_sensor_id("105");
    
    // 1. Explicit Check
    if (id.has_value()) { // Or simply: if (id)
        std::cout << "Sensor ID: " << id.value() << "\n"; // Or: *id
    }
    
    // 2. Default Fallback
    // If id is empty, fallback to 0
    int active_id = id.value_or(0); 
    
    return 0;
}
