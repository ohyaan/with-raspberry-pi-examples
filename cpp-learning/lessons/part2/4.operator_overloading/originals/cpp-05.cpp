#include <algorithm>

class CustomArray {
    size_t size;
    int* data;
public:
    CustomArray(size_t s) : size(s), data(new int[s]) {}
    ~CustomArray() { delete[] data; }
    
    // Copy Constructor
    CustomArray(const CustomArray& other) : size(other.size), data(new int[other.size]) {
        std::copy(other.data, other.data + size, data);
    }

    // Friend swap function
    friend void swap(CustomArray& first, CustomArray& second) noexcept {
        using std::swap;
        swap(first.size, second.size);
        swap(first.data, second.data);
    }

    // Copy Assignment Operator using Copy-and-Swap
    // Note: passing by value triggers the copy constructor automatically!
    CustomArray& operator=(CustomArray other) {
        swap(*this, other); // Swap local copy contents with this
        return *this;
    } // 'other' goes out of scope here, automatically deleting old memory
};
