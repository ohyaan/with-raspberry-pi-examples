int x = 10, y = 20;
int* const ptr = &x;

*ptr = 15;   // OK: data can change
// ptr = &y; // ERROR: pointer address is read-only
