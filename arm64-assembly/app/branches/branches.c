/* file: branches.c */
#include <stdint.h>
#include <stdio.h>
extern uint64_t sum_to_n(uint64_t);
extern int signed_less(int64_t, int64_t);
extern int unsigned_less(uint64_t, uint64_t);
int main(void)
{
    const uint64_t inputs[] = {0, 1, 3, 10, 1000};
    for (unsigned i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
        uint64_t n = inputs[i];
        if (sum_to_n(n) != n * (n + 1) / 2) return 1;
    }
    if (signed_less(-1, 1) != 1 ||
        unsigned_less(UINT64_MAX, 1) != 0 ||
        signed_less(7, 7) != 0 || unsigned_less(7, 7) != 0) return 1;
    puts("PASS branches: empty, bounded loop, signed and unsigned");
    return 0;
}
