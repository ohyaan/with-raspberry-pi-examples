/* file: addresses.c */
#include <stdint.h>
#include <stdio.h>
extern const uint64_t *value_address(void);
extern uint64_t read_value(void);
extern uint64_t literal_value(void);
int main(void)
{
    const uint64_t *p = value_address();
    if (*p != 42 || read_value() != 42 || literal_value() != 42) return 1;
    puts("PASS addresses: pointer, dereference and literal value");
    return 0;
}
