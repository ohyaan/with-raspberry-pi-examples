#include <algorithm>

class Buffer {
    size_t size;
    int* data;
public:
    Buffer(size_t s) : size(s), data(new int[s]) {}
    
    // 1. Destructor (Releases memory)
    ~Buffer() { delete[] data; }
    
    // 2. Copy Constructor (Deep copy logic)
    Buffer(const Buffer& other) : size(other.size), data(new int[other.size]) {
        std::copy(other.data, other.data + size, data);
    }
    
    // 3. Copy Assignment Operator (Protects against self-assignment)
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            delete[] data;
            size = other.size;
            data = new int[size];
            std::copy(other.data, other.data + size, data);
        }
        return *this;
    }
};
