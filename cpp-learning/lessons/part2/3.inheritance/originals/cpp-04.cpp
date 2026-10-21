#include <iostream>

class BadBase {
public:
    ~BadBase() { std::cout << "BadBase destroyed\n"; }
};

class Derived : public BadBase {
    int* buffer;
public:
    Derived() : buffer(new int[100]) {}
    ~Derived() { 
        delete[] buffer; 
        std::cout << "Derived destroyed\n"; 
    }
};

int main() {
    BadBase* ptr = new Derived();
    delete ptr; // LEAK! Only prints "BadBase destroyed", Derived destructor never runs.
}
