class Vector2D {
public:
    double x, y;
    Vector2D(double x, double y) : x(x), y(y) {}

    // Binary + operator overloaded as member function
    Vector2D operator+(const Vector2D& other) const {
        return Vector2D(x + other.x, y + other.y);
    }
};

Vector2D v1{1.0, 2.0};
Vector2D v2{3.0, 4.0};
Vector2D v3 = v1 + v2; // Equivalent to: v1.operator+(v2)
