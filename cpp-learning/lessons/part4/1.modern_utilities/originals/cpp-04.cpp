#include <any>
#include <string>
#include <iostream>

int main() {
    std::any container = 100; // Stores int
    container = std::string("Flexible storage"); // Stores string
    
    try {
        std::string text = std::any_cast<std::string>(container);
        std::cout << text << "\n";
    } catch (const std::bad_any_cast& e) {
        std::cout << "Invalid cast: " << e.what() << "\n";
    }
    
    return 0;
}
