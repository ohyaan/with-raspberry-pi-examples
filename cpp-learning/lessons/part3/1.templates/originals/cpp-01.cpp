#include <iostream>

// Declare the template parameter T
template <typename T>
T find_max(T a, T b) {
    return (a > b) ? a : b;
}

int main() {
    // 1. Automatic Type Deduction
    int i = find_max(10, 20);       // Compiler deduces T as int
    double d = find_max(3.14, 2.71); // Compiler deduces T as double
    
    // 2. Explicit Type Specification
    // Forces conversion of parameters to double
    double mix = find_max<double>(10, 2.5); 
    
    return 0;
}
