void log(int x);     // Overload A
void log(int* ptr);   // Overload B

log(NULL); // Ambiguous! NULL is an integer, so this might call Overload A instead of B.
log(nullptr); // Safe! 'nullptr' has type 'std::nullptr_t', correctly calling Overload B.
