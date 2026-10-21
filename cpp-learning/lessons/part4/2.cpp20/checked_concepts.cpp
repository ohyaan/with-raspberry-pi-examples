// Supplementary driver for original constraints; no module import is claimed.
#include "originals/cpp-01.cpp"
#include "originals/cpp-02.cpp"
#include "originals/cpp-03.cpp"
int main() {
    if (add_old(2, 3) != 5 || add_modern(4, 5) != 9 || add_clause(6, 7) != 13) return 1;
    log_debug(42);
    return 0;
}
