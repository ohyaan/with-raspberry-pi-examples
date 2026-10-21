#include <iostream>
#include <string>
#include <limits>

int main() {
    std::cout << "Enter your age: ";
    int age = 0;
    std::cin >> age; // Extraction operator

    // Clear input buffer to handle remaining newline
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << "Enter your full name: ";
    std::string full_name;
    std::getline(std::cin, full_name); // Reads the whole line including spaces

    std::cout << "\nHello, " << full_name << "! You are " << age << " years old.\n";
    return 0;
}
