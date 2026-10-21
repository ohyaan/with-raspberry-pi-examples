#include <iostream>
#include <string>

class Device {
protected:
    std::string name;
public:
    Device(const std::string& n) : name(n) {}
    void power_on() { std::cout << name << " powering on.\n"; }
};

// Publicly inherits from Device
class Camera : public Device {
    int resolution;
public:
    Camera(const std::string& n, int res) : Device(n), resolution(res) {}
    void capture() { std::cout << "Capturing at " << resolution << "MP.\n"; }
};
