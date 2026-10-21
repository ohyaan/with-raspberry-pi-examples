#include <ranges>
#include <vector>
#include <iostream>

namespace views = std::views;

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8};
    
    // Lazy evaluation pipeline:
    // 1. Filter even numbers: {2, 4, 6, 8}
    // 2. Drop the first element: {4, 6, 8}
    // 3. Multiply by 10: {40, 60, 80}
    auto pipeline = nums 
                  | views::filter([](int n) { return n % 2 == 0; })
                  | views::drop(1)
                  | views::transform([](int n) { return n * 10; });
                  
    for (int val : pipeline) {
        std::cout << val << " "; // Prints: 40 60 80
    }
    std::cout << "\n";
    
    return 0;
}
