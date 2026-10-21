// Abstract Base Class
class Sensor {
public:
    virtual ~Sensor() = default; // Mandatory virtual destructor
    
    // Pure virtual function
    virtual double read() = 0; 
};

class TemperatureSensor : public Sensor {
public:
    double read() override {
        return 24.3; // Must implement to allow instantiation
    }
};
