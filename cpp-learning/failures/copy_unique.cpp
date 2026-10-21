#include "../lessons/part2/1.memory_management/originals/cpp-04.cpp"
int main() { auto first = std::make_unique<Device>(); auto second = first; }
