#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> nums = {12, 5, 8, 23, 19};
    
    // Find value 8
    auto it = std::find(nums.begin(), nums.end(), 8);
    if (it != nums.end()) {
        std::cout << "Found value: " << *it << "\n";
    }
    
    // Find the first value greater than 15 using a lambda predicate
    auto it_gt = std::find_if(nums.begin(), nums.end(), [](int n) {
        return n > 15;
    });
    
    if (it_gt != nums.end()) {
        std::cout << "First value > 15: " << *it_gt << "\n"; // Prints 23
    }
    
    return 0;
}
