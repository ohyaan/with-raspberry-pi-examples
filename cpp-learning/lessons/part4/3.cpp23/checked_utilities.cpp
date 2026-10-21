// Supplementary checks for the article's C++23 byte/enum examples.
#include "originals/cpp-06.cpp"
#include "originals/cpp-07.cpp"
int main() {
    if (std::byteswap(std::uint32_t{0x12345678}) != std::uint32_t{0x78563412}) return 1;
    if (std::to_underlying(Status::Error) != 1) return 1;
    return 0;
}
