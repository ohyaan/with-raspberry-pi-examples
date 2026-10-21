#include <iostream>

class Shape {
public:
    virtual void draw() const {
        std::cout << "Drawing generic shape.\n";
    }
    virtual ~Shape() = default;
};

class Circle : public Shape {
public:
    // C++11 override: guarantees a base method signature is matched
    void draw() const override {
        std::cout << "Drawing a circle.\n";
    }
};

void render(const Shape& shape) {
    shape.draw(); // Resolves at runtime to call the correct function
}
