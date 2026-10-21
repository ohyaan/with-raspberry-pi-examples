#include <vector>
#include <iostream>

int main() {
    std::vector<int> nums = {10, 20, 30};

    // Forward iterator traversal
    for (auto it = nums.begin(); it != nums.end(); ++it) {
        std::cout << *it << " "; // Dereference iterator
    }
    std::cout << "\n";
    
    // Constant iterator (Read-only)
    std::vector<int>::const_iterator cit = nums.cbegin();
    
    // Reverse iterator (Traverse backwards)
    for (auto rit = nums.rbegin(); rit != nums.rend(); ++rit) {
        std::cout << *rit << " "; // Prints: 30 20 10
    }
    std::cout << "\n";
}
