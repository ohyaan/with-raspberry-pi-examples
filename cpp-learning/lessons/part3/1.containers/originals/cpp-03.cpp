#include <iostream>
#include <map>
#include <unordered_map>
#include <string>

int main() {
    // 1. std::map: Sorted order
    std::map<std::string, int> ages;
    ages["Charlie"] = 30;
    ages["Alice"] = 25;
    
    for (const auto& [name, age] : ages) {
        std::cout << name << ": " << age << "\n"; 
        // Output is sorted: Alice then Charlie
    }

    // 2. std::unordered_map: Unsorted, fast lookups
    std::unordered_map<int, std::string> error_codes;
    error_codes[404] = "Not Found";
    error_codes[200] = "OK";
    
    return 0;
}
