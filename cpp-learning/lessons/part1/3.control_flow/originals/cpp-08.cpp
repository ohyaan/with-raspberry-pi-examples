#include <vector>
#include <string>

std::vector<std::string> messages = {"Init", "Process", "Cleanup"};

// 1. Iteration by Value (Copies each string - slow for large objects)
for (auto msg : messages) {
    std::cout << msg << "\n";
}

// 2. Iteration by Const Reference (Read-only, zero copying - highly recommended)
for (const auto& msg : messages) {
    std::cout << msg << "\n";
}

// 3. Iteration by Non-Const Reference (Allows modifying elements)
for (auto& msg : messages) {
    msg += " [OK]";
}
