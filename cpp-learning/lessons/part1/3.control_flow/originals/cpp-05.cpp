#include <iostream>
#include <type_traits>

template <typename T>
void process_input(T value) {
    if constexpr (std::is_pointer_v<T>) {
        std::cout << "Processing a pointer. Address: " << value 
                  << ", Value: " << *value << "\n";
    } else {
        std::cout << "Processing a direct value: " << value << "\n";
    }
}

int main() {
    int num = 100;
    process_input(num);   // Keeps the 'else' block, discards the 'if'
    process_input(&num);  // Keeps the 'if' block, discards the 'else'
    return 0;
}
