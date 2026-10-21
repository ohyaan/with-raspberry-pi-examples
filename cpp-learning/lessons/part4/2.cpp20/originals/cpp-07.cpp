#include <format>
#include <string>
#include <iostream>

int main() {
    double temp = 24.567;
    int pin = 18;
    
    // Clean Python-like formatting
    std::string s = std::format("GPIO Pin {} temp is {:.1f}C\n", pin, temp);
    std::cout << s; // Prints: GPIO Pin 18 temp is 24.6C
    
    return 0;
}
