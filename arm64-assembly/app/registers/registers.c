/* file: registers.c */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
extern uint64_t width_demo(void);
extern uint64_t constant_demo(void);
extern uint64_t scaled_sum(uint64_t, uint64_t, uint64_t);
_Static_assert(sizeof(void *) == 8, "Use a 64-bit userspace");
int main(void)
{
    uint64_t width = width_demo();
    uint64_t constant = constant_demo();
    uint64_t sum = scaled_sum(10, 20, 3);
    if (width != UINT64_C(0x1234) ||
        constant != UINT64_C(0x123456789abcdef0) || sum != 90) {
        fputs("Register lab check failed\n", stderr);
        return 1;
    }
    printf("width: 0x%016" PRIx64 "\nconstant: 0x%016" PRIx64
           "\nscaled_sum: %" PRIu64 "\n", width, constant, sum);
    return 0;
}
