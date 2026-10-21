#include <concepts>
#include <iostream>

// A type is Printable if it supports stream insertion
template <typename T>
concept Printable = requires(std::ostream& os, T value) {
    { os << value } -> std::same_as<std::ostream&>; 
};

template <Printable T>
void log_debug(const T& val) {
    std::cout << val << "\n";
}
