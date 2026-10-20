/* file: interop.c */
#include <stdint.h>
#include <stdio.h>
extern uint64_t add_u64(uint64_t, uint64_t);
int main(void)
{
    if (add_u64(2, 3) != 5 || add_u64(UINT64_MAX, 1) != 0) return 1;
    puts("PASS C linkage: unsigned wrap is specified");
    return 0;
}
