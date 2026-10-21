#include <algorithm>

class Buffer {
    size_t size;
    int* data;
public:
    Buffer(size_t s) : size(s), data(new int[s]) {}
    ~Buffer() { delete[] data; }
    Buffer(const Buffer& other) : size(other.size), data(new int[other.size]) {
        std::copy(other.data, other.data + size, data);
    }
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            delete[] data;
            size = other.size;
            data = new int[size];
            std::copy(other.data, other.data + size, data);
        }
        return *this;
    }

    // 4. Move Constructor (Transfer pointer, leave source empty)
    Buffer(Buffer&& other) noexcept : size(other.size), data(other.data) {
        other.size = 0;
        other.data = nullptr; // Reset source object
    }

    // 5. Move Assignment Operator
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data; // Free existing resource
            
            size = other.size;
            data = other.data; // Take over resource
            
            other.size = 0;
            other.data = nullptr; // Reset source object
        }
        return *this;
    }
};
