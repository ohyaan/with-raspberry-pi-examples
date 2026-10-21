// 1. Auto deduction
auto add(int a, int b) {
    return a + b; // Deduced as int
}

// 2. Trailing return type
auto multiply(double a, double b) -> double {
    return a * b;
}
