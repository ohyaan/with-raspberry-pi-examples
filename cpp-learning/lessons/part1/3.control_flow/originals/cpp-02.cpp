// Traditional style: 'status' leaks to the parent scope
int status = get_sensor_status();
if (status == 0) {
    std::cout << "Sensor healthy\n";
}
// 'status' is still visible and mutable here, which might cause bugs.

// C++17 Style: 'status' is confined to the if/else block
if (int status = get_sensor_status(); status == 0) {
    std::cout << "Sensor healthy\n";
} else {
    std::cout << "Sensor error code: " << status << "\n";
}
// Compilation error: 'status' is out of scope here
