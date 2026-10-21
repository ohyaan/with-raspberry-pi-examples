// Supplementary driver: only safe base classes with virtual destructors.
#include "originals/cpp-01.cpp"
#include "originals/cpp-02.cpp"
#include "originals/cpp-03.cpp"
#include "originals/cpp-07.cpp"
int main() {
    Camera camera("Pi", 8);
    camera.power_on();
    camera.capture();
    Circle circle;
    render(circle);
    TemperatureSensor sensor;
    if (sensor.read() != 24.3) return 1;
    Dog dog;
    Cat cat;
    play_with_animal(&dog);
    play_with_animal(&cat);
    return 0;
}
