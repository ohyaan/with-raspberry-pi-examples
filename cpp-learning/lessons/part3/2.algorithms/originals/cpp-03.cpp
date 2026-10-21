#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> numbers = {1, 2, 3, 4};
    
    // Square every element in-place
    std::transform(numbers.begin(), numbers.end(), numbers.begin(), [](int n) {
        return n * n;
    });
    
    // numbers is now: {1, 4, 9, 16}
    return 0;
}
