#include <bit>
#include <cstdint>

void endian_swap() {
    uint32_t network_val = 0x12345678;
    uint32_t host_val = std::byteswap(network_val); // Swaps bytes to 0x78563412
}
