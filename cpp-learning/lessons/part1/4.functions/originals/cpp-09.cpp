consteval int force_compile_time_math(int x) {
    return x * x;
}

int main() {
    constexpr int ans = force_compile_time_math(5); // OK
    
    // int run_val = 5;
    // int ans2 = force_compile_time_math(run_val); 
    // ERROR: run_val is not a compile-time constant!
}
