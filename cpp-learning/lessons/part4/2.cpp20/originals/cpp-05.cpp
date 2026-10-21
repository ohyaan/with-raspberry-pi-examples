export module math; // Declare module name

export int add(int a, int b) { // Export public functions
    return a + b;
}

int internal_helper() { // Non-exported private function
    return 42;
}
