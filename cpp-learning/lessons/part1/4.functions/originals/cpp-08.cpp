constexpr int add_constexpr(int a, int b) {
    return a + b;
}

int main() {
    // Evaluated at compile-time (optimizes to direct constant placement)
    constexpr int val = add_constexpr(10, 20); 
    static_assert(val == 30, "Validation check");
    
    // Evaluated at runtime
    int x = 5;
    int y = add_constexpr(x, 10); 
}
