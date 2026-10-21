#include <iostream>

int main() {
    int counter = 10;
    
    // Lambda capturing by value. Without 'mutable', 'counter++' would fail compilation.
    auto increment = [counter]() mutable {
        counter++; 
        std::cout << "Inside lambda: " << counter << "\n"; // Prints 11
    };
    
    increment();
    std::cout << "Outside lambda: " << counter << "\n"; // Prints 10 (unchanged)
    
    return 0;
}
