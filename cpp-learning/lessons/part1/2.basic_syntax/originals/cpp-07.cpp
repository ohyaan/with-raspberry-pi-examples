#include <iostream>
#include <tuple>

struct Point {
    double x;
    double y;
};

int main() {
    Point pt{3.5, 4.2};
    
    // Unpack struct members directly into two variables
    auto [posX, posY] = pt; 
    
    std::cout << "X: " << posX << ", Y: " << posY << "\n";
    
    // Works with std::pair / std::tuple as well
    std::pair<int, std::string> error{404, "Not Found"};
    auto [code, message] = error;
    
    return 0;
}
