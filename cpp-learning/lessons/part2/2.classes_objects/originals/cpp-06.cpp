class Math {
public:
    static double calculate_pi() { return 3.14159265; }
};

int main() {
    double pi = Math::calculate_pi(); // No object needed
}
