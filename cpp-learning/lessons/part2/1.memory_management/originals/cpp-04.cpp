#include <memory>
#include <iostream>

struct Device {
    void ping() { std::cout << "Device active\n"; }
};

void run_device() {
    // Preferred creation method (Exception-safe and clean)
    std::unique_ptr<Device> dev = std::make_unique<Device>();
    dev->ping();
    
    // std::unique_ptr<Device> copy_dev = dev; // Compile Error! Copying is forbidden.
    
    std::unique_ptr<Device> moved_dev = std::move(dev); // OK. 'dev' is now null.
} // 'moved_dev' goes out of scope here; Device is automatically deleted.
