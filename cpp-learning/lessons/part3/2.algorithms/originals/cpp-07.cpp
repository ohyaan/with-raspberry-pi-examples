#include <ranges>
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    // 1. Direct container sorting
    std::ranges::sort(numbers);

    // 2. Chained lazy evaluations
    // Filter even numbers and square them
    auto pipeline = numbers 
                  | std::views::filter([](int n) { return n % 2 == 0; })
                  | std::views::transform([](int n) { return n * n; });
                  
    // Elements are computed one-by-one only during the iteration
    for (int n : pipeline) {
        std::cout << n << " "; // Prints: 4 16 36 64 100
    }
    std::cout << "\n";
    
    return 0;
}
