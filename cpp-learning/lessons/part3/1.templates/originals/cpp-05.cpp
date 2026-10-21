#include <iostream>

// Primary (Generic) Template
template <typename T>
void print_type(T value) {
    std::cout << value << " is a generic value.\n";
}

// Full Specialization for bool
template <>
void print_type<bool>(bool value) {
    std::cout << (value ? "true" : "false") << " is a boolean value.\n";
}
