#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> data = {9, 2, 5, 1, 7};
    
    // 1. Sort ascending
    std::sort(data.begin(), data.end()); 
    // data is now: {1, 2, 5, 7, 9}
    
    // 2. Sort descending using a standard comparator
    std::sort(data.begin(), data.end(), std::greater<int>());
    // data is now: {9, 7, 5, 2, 1}
    
    // 3. Binary Search (Requires range to be sorted first)
    std::sort(data.begin(), data.end());
    bool has_five = std::binary_search(data.begin(), data.end(), 5); // O(log n)
    std::cout << "Has 5: " << std::boolalpha << has_five << "\n";
    
    return 0;
}
