#include <variant>
#include <iostream>
#include <string>

using Payload2 = std::variant<int, double, std::string>;

struct Printer {
    void operator()(int i) const { std::cout << "Integer: " << i << "\n"; }
    void operator()(double d) const { std::cout << "Double: " << d << "\n"; }
    void operator()(const std::string& s) const { std::cout << "String: " << s << "\n"; }
};

int main() {
    Payload2 payload = 3.14;
    
    // Calls the correct overload automatically
    std::visit(Printer{}, payload); 
    
    return 0;
}
