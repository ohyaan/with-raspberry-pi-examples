#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

class Sensor {
    int reading;
    mutable unsigned hits = 0;

public:
    explicit Sensor(int value) : reading(value) {}
    int inspect() const { ++hits; return reading; }
    void set(int value) { reading = value; }
    unsigned inspections() const { return hits; }
};

std::string_view label(const std::string& owner) {
    return owner;
}

int main() {
    Sensor sensor(46700);
    const Sensor& view = sensor;
    const int before = view.inspect();
    sensor.set(-500);
    const int after = view.inspect();
    assert(before == 46700 && after == -500);
    assert(sensor.inspections() == 2);
    std::cout << "before=" << before << ", after=" << after << '\n';
    std::cout << "inspections=" << sensor.inspections() << '\n';

    std::string owner = "Pi 4B";
    const auto borrowed = label(owner);
    assert(borrowed.size() == 5);
    std::cout << "label bytes=" << borrowed.size() << '\n';

    int plain = 0;
    const int& observed = plain;
    plain = 1;
    assert(observed == 1);
    std::cout << "after borrow=" << plain << '\n';

    const Sensor fixed(0);
    assert(fixed.inspect() == 0);
    assert(fixed.inspections() == 1);
}
