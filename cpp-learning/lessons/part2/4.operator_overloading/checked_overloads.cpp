// Supplementary driver: exercise safe value/stream operators, not raw-array copies.
#include <sstream>
#include <memory>
#include <type_traits>
#include "originals/cpp-01.cpp"
#include "originals/cpp-03.cpp"
#include "originals/cpp-04.cpp"
int main() {
    if (v3.x != 4.0 || v3.y != 6.0) return 1;
    User user("Pi", 4);
    std::istringstream input("Ada 25");
    input >> user;
    std::ostringstream output;
    output << user;
    if (output.str() != "User: Ada (Age: 25)") return 1;
    Counter count;
    static_assert(std::is_same_v<decltype(++count), Counter&>);
    static_assert(std::is_same_v<decltype(count++), Counter>);
    if (std::addressof(++count) != &count) return 1;
    std::cout << "PASS checked_overloads\n";
}
