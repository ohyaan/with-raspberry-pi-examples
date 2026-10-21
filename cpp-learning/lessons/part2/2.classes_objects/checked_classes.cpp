// Supplementary driver: value-initialize the Sensor; do not run raw Buffer copies.
#include <iostream>
#include "originals/cpp-01.cpp"
#include "originals/cpp-02.cpp"
#include "originals/cpp-03.cpp"
#include "originals/cpp-04.cpp"
#include "originals/cpp-05.cpp"
int main() {
    Point p{3.0, 4.0};
    if (p.x != 3.0 || p.y != 4.0) return 1;
    Servo servo;
    Servo other(17, 600, 2400);
    User user("Pi");
    User2 second("Pi");
    Sensor sensor{};
    if (sensor.get_reading() != 0.0) return 1;
    Logger first;
    Logger last;
    if (Logger::log_count != 2) return 1;
    std::cout << "PASS checked_classes\n";
}
