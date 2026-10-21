#include <iostream>
#include <memory>

struct Node {
    std::shared_ptr<Node> partner;
    ~Node() { std::cout << "Node destroyed\n"; }
};

void create_leak() {
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    
    a->partner = b; // b ref count = 2
    b->partner = a; // a ref count = 2
} // a and b stack variables go out of scope. 
  // ref counts decrement to 1. Neither destructor runs! Memory is leaked.
