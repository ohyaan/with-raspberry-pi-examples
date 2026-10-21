#include <numeric>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};
    
    // Calculate sum of elements (initial value is 0)
    int sum = std::accumulate(nums.begin(), nums.end(), 0); // 15
    
    // Calculate product of elements (initial value is 1)
    int product = std::accumulate(nums.begin(), nums.end(), 1, std::multiplies<int>()); // 120
    
    return 0;
}
