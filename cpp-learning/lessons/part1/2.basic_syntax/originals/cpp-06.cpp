int get_sensor_reading() { return 42; } // Evaluated at runtime

int main() {
    const int val1 = get_sensor_reading(); // OK: determined at runtime
    
    // constexpr int val2 = get_sensor_reading(); 
    // ERROR: get_sensor_reading() cannot be evaluated at compile-time!
    
    constexpr int max_buffer = 1024 * 4; // OK: compile-time constant
}
