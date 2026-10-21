#include <iostream>

template <typename T>
class Wrapper2 {
    T data;
public:
    Wrapper2(T val) : data(val) {}
};

// Specialization for all pointer types
template <typename T>
class Wrapper2<T*> {
    T* data;
public:
    Wrapper2(T* val) : data(val) {}
    void print_dereferenced() const {
        if (data) std::cout << *data << "\n";
    }
};
