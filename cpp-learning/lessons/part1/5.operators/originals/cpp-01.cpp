int a = 11;
int b = 3;

int quotient = a / b;  // Result: 3 (fractional part discarded)
int remainder = a % b; // Result: 2

// To get exact decimal division, at least one operand must be cast to double
double exact = static_cast<double>(a) / b; // Result: 3.66667
