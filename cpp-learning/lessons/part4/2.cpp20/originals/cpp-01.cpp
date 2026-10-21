#include <type_traits>

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T add_old(T a, T b) {
    return a + b;
}
