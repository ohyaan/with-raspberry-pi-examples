class Servo {
    int pin = 18;       // In-class default initializer
    int min_pulse = 500;
    int max_pulse = 2500;
public:
    // Default constructor uses default values
    Servo() = default; 
    
    // Parameterized constructor delegating to default values
    Servo(int p) : pin(p) {} 
    
    // Delegating constructor
    Servo(int p, int min, int max) : pin(p), min_pulse(min), max_pulse(max) {}
};
