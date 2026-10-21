#include "sensor.hpp"
#include <iostream>
int main() {
    if (fixture_reading() != 42) return 1;
    std::cout << "fixture=42\n";
}
