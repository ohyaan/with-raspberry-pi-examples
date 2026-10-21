#include <iostream>

// C++17 Fold Expression: Sum all arguments recursively
template <typename... Args>
auto sum_all(Args... args) {
    return (args + ...); // Unpacks to: arg1 + arg2 + ... + argN
}

int main() {
    auto result = sum_all(1, 2.5, 3, 4.2); // Unpacks and sums as double
    std::cout << "Sum: " << result << "\n"; // Prints 10.7
    return 0;
}
