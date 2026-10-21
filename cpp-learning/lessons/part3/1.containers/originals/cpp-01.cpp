#include <vector>

void fill_vector() {
    std::vector<int> vec;
    vec.reserve(10000); // Allocates memory for 10,000 items in one call
    
    for (int i = 0; i < 10000; ++i) {
        vec.push_back(i); // Zero reallocations!
    }
}
