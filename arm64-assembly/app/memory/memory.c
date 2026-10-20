/* file: memory.c */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
extern int64_t sum_i32(const int32_t *, uint64_t);
extern uint64_t zero_word(const uint32_t *);
int main(void)
{
    const int32_t values[] = {-1, 2, -3, 4};
    const uint32_t word = UINT32_MAX;
    if (sum_i32(0, 0) != 0 || sum_i32(values, 1) != -1 ||
        sum_i32(values, 4) != 2 ||
        zero_word(&word) != UINT64_C(0xffffffff)) return 1;
    printf("sum: %" PRId64 "\n", sum_i32(values, 4));
    puts("PASS memory: empty, signed loads, zero extension");
    return 0;
}
