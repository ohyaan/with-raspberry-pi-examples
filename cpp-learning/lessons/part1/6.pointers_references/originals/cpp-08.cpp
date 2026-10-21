#include <iostream>

int main() {
    int arr[] = {10, 20, 30, 40};
    int* ptr = arr; // Points to arr[0] (value 10)
    
    std::cout << "arr[0] via pointer: " << *ptr << "\n"; // Prints 10
    
    // Pointer Arithmetic: increments address by sizeof(int) (4 bytes)
    ptr++; 
    std::cout << "arr[1] via pointer: " << *ptr << "\n"; // Prints 20
    
    // Access with offset index
    std::cout << "arr[3] via offset: " << *(ptr + 2) << "\n"; // Prints 40
    
    return 0;
}
