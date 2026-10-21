#include <memory>
#include <iostream>

struct Node {
    std::weak_ptr<Node> partner; // Observer. Resolves cyclic reference.
    ~Node() { std::cout << "Node destroyed\n"; }
};
