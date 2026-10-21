// Supplementary driver for existing class/specialization fragments.
#include "originals/cpp-02.cpp"
#include "originals/cpp-03.cpp"
#include "originals/cpp-04.cpp"
#include "originals/cpp-05.cpp"
#include "originals/cpp-06.cpp"
int main() {
    if (wrap1.get_value() != 42 || wrap2.get_value() != 42) return 1;
    print_type(4);
    print_type(true);
    int number = 9;
    Wrapper2<int*> pointer(&number);
    pointer.print_dereferenced();
    return 0;
}
