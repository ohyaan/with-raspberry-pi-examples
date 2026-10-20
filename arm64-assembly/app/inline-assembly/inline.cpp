// file: inline.cpp
#include <cstdint>
#include <iostream>
#include <limits>

static std::uint64_t add_inline(std::uint64_t a, std::uint64_t b)
{
    std::uint64_t result;
    asm ("add %x[out], %x[left], %x[right]"
         : [out] "=r" (result)
         : [left] "r" (a), [right] "r" (b));
    return result;
}

int main()
{
    const std::uint64_t values[] = {0, 1, 17,
        std::numeric_limits<std::uint64_t>::max()};
    for (auto a : values)
        for (auto b : values)
            if (add_inline(a, b) != a + b) return 1;
    std::cout << "PASS inline: 16 unsigned input pairs\n";
}
