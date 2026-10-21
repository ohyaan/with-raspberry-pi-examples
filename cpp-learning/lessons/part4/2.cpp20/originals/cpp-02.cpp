#include <concepts>

// 1. Direct Constraint on type parameter
template <std::integral T>
T add_modern(T a, T b) {
    return a + b;
}

// 2. Or using the 'requires' clause
template <typename T>
requires std::integral<T>
T add_clause(T a, T b) {
    return a + b;
}
