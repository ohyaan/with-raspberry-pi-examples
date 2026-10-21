#include <iostream>

int main() {
    int num = 42;
    
    // Declare a pointer to an integer, initialized to the address of 'num'
    int* ptr = &num; 
    
    std::cout << "Value of num: " << num << "\n";          // Prints: 42
    std::cout << "Address of num (&num): " << &num << "\n";  // Prints: e.g., 0x7ffd51a0
    std::cout << "Pointer value (ptr): " << ptr << "\n";    // Prints: e.g., 0x7ffd51a0
    
    // Dereference pointer to read value
    std::cout << "Value pointed to (*ptr): " << *ptr << "\n"; // Prints: 42
    
    // Dereference pointer to write value
    *ptr = 99;
    std::cout << "New value of num: " << num << "\n";      // Prints: 99
    
    return 0;
}
