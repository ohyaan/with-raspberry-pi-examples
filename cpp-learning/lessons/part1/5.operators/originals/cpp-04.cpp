bool check_sensor(); // Expensive function

// If 'is_active' is false, 'check_sensor()' is never called, saving CPU cycles.
if (is_active && check_sensor()) {
    // ...
}
