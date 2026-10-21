#include <iostream>
#include <compare>

struct Version {
    int major;
    int minor;
    int patch;

    // Direct compiler-generated member-by-member comparison
    auto operator<=>(const Version&) const = default;
};

int main() {
    Version v1{1, 2, 0};
    Version v2{1, 3, 1};

    // All comparison operators are automatically generated and work:
    if (v1 < v2) {
        std::cout << "v1 is older than v2\n";
    }
    if (v1 != v2) {
        std::cout << "Versions are different\n";
    }
    
    return 0;
}
