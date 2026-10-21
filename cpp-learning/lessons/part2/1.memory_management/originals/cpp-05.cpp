#include <memory>
#include <iostream>

void shared_demo() {
    std::shared_ptr<int> p1 = std::make_shared<int>(100); // ref_count = 1
    {
        std::shared_ptr<int> p2 = p1; // ref_count = 2
        std::cout << *p2 << "\n";
    } // p2 goes out of scope, ref_count = 1
} // p1 goes out of scope, ref_count = 0, heap memory is deleted.
