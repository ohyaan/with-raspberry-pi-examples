// Supplementary driver: original statement fragments, each in its own scope.
int main() {
    {
#include "originals/cpp-01.cpp"
        if (quotient != 3 || remainder != 2 || exact <= 3.0 || exact >= 4.0) return 1;
    }
    {
#include "originals/cpp-02.cpp"
        if (x != 30) return 1;
    }
    {
#include "originals/cpp-03.cpp"
        if (x != 6 || y != 6 || a != 6 || b != 5) return 1;
    }
    return 0;
}
