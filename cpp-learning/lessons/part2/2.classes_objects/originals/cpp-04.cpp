class Sensor {
    double reading;
public:
    // Read-only method
    double get_reading() const {
        // reading = 0.0; // Compile Error! Cannot modify inside const method.
        return reading;
    }
};
