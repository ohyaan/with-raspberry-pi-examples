#include <memory>
#include <iostream>

void weak_demo(std::shared_ptr<int> shared) {
    std::weak_ptr<int> weak = shared;
    
    // Check if the object still exists before accessing
    if (std::shared_ptr<int> locked = weak.lock()) {
        std::cout << "Value: " << *locked << "\n";
    } else {
        std::cout << "Resource has already been deleted.\n";
    }
}
