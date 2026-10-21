int x = 10, y = 20;
const int* ptr = &x;

// *ptr = 15; // ERROR: data is read-only
ptr = &y;     // OK: pointer can change addresses
