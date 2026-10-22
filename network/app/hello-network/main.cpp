#include <iostream>

int main() {
    static_assert(__cplusplus >= 202002L, "Build this course as C++20");
    std::cout << "Network lab ready\nC++20: yes\n";
}
