// Supplementary driver; no cycle/leak or uninitialized array example is executed.
#include "originals/cpp-04.cpp"
#include "originals/cpp-05.cpp"
#include "originals/cpp-06.cpp"
#include "originals/cpp-08.cpp"
int main() {
    run_device();
    shared_demo();
    auto value = std::make_shared<int>(7);
    weak_demo(value);
    weak_demo({});
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->partner = b;
    b->partner = a;
    if (a.use_count() != 1 || b.use_count() != 1) return 1;
    b.reset();
    if (!a->partner.expired()) return 1;
    a.reset();
    return 0;
}
