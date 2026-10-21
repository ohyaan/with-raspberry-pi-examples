#include <vector>
#include <iostream>

int main() {
    std::vector<int> source = {1, 2, 3, 4, 5};
    
    // Moves data from source to destination.
    // Underlying array pointer is copied, and source is set to empty.
    std::vector<int> destination = std::move(source); 
    
    std::cout << "Source size: " << source.size() << "\n";           // Prints 0
    std::cout << "Destination size: " << destination.size() << "\n"; // Prints 5
    
    return 0;
}
