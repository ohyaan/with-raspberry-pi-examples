// file: interop.cpp
#include <cstdint>
#include <iostream>
#include <limits>
extern "C" std::uint64_t add_u64(std::uint64_t, std::uint64_t);
int main()
{
    if (add_u64(2, 3) != 5 ||
        add_u64(std::numeric_limits<std::uint64_t>::max(), 1) != 0) return 1;
    std::cout << "PASS C++ linkage: same assembly object\n";
}
