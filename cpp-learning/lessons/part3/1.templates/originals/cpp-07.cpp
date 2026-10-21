#include <iostream>

// 'Limit' is a non-type template parameter
template <typename T, size_t Limit>
class StaticArray {
    T data[Limit]; // Size is allocated on the stack at compile-time
public:
    size_t size() const { return Limit; }
};

int main() {
    StaticArray<int, 256> buffer;
    std::cout << "Buffer capacity: " << buffer.size() << "\n"; // Prints 256
}
