// Supports: Vector2D v2 = v1 * 2.0; (Member function works)
// But what about: Vector2D v2 = 2.0 * v1;? 
// The left operand is double, so double.operator*(Vector2D) is impossible.

class Vector2D {
    double x, y;
public:
    Vector2D(double x, double y) : x(x), y(y) {}

    // Declare non-member function as friend to access private members
    friend Vector2D operator*(double scalar, const Vector2D& vec);
};

// Non-member definition
Vector2D operator*(double scalar, const Vector2D& vec) {
    return Vector2D(vec.x * scalar, vec.y * scalar);
}
