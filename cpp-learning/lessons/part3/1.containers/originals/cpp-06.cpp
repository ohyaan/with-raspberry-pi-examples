#include <span>
#include <vector>
#include <array>
#include <iostream>

// Accepts any contiguous buffer containing integers
void invert_buffer(std::span<int> data) {
    for (int& val : data) {
        val = -val;
    }
}

int main() {
    int raw_arr[] = {1, 2, 3};
    std::vector<int> vec = {4, 5, 6};
    std::array<int, 3> arr = {7, 8, 9};

    invert_buffer(raw_arr); // Works!
    invert_buffer(vec);     // Works!
    invert_buffer(arr);     // Works!
    
    return 0;
}
