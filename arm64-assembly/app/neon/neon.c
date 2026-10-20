/* file: neon.c */
#include <stdint.h>
#include <stdio.h>
extern void add_one_u32(const uint32_t *, uint32_t *, uint64_t);
int main(void)
{
    const uint32_t input[9] = {0, 1, UINT32_MAX, 3, 4, 5, 6, 7, 8};
    add_one_u32(0, 0, 0);
    for (unsigned n = 0; n <= 9; ++n) {
        uint32_t output[11];
        for (unsigned i = 0; i < 11; ++i) output[i] = UINT32_C(0xdeadbeef);
        add_one_u32(input, output + 1, n);
        if (output[0] != UINT32_C(0xdeadbeef)) return 1;
        for (unsigned i = 0; i < n; ++i)
            if (output[i + 1] != (uint32_t)(input[i] + UINT32_C(1))) return 1;
        for (unsigned i = n + 1; i < 11; ++i)
            if (output[i] != UINT32_C(0xdeadbeef)) return 1;
    }
    puts("PASS NEON: lengths 0..9, wrap and output guards");
    return 0;
}
