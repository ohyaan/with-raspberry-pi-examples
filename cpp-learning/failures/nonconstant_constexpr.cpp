// Adapter activating the article's commented constexpr failure.
int get_sensor_reading() { return 42; }
int main() { constexpr int value = get_sensor_reading(); return value; }
