/* file: functions.c */
#include <stdint.h>
#include <stdio.h>
extern uint64_t double_plus(uint64_t, uint64_t);
int main(void)
{
    if (double_plus(10, 7) != 27 || double_plus(0, 0) != 0 ||
        double_plus(UINT64_MAX, 1) != UINT64_MAX) return 1;
    puts("PASS functions: nested call and live argument");
    return 0;
}
