/* file: report.c */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
extern int64_t summarize_i32(const int32_t *, uint64_t, uint64_t *);
static int checked_summary(const int32_t *p, size_t n,
                           int64_t *sum, uint64_t *negative)
{
    if (!sum || !negative || n > 1024 || (n && !p)) return 0;
    *sum = summarize_i32(p, n, negative);
    return 1;
}
static int check(const int32_t *p, size_t n)
{
    int64_t expected_sum = 0, actual_sum = 0;
    uint64_t expected_negative = 0, actual_negative = UINT64_MAX;
    for (size_t i = 0; i < n; ++i) {
        expected_sum += p[i];
        expected_negative += p[i] < 0;
    }
    return checked_summary(p, n, &actual_sum, &actual_negative) &&
           actual_sum == expected_sum && actual_negative == expected_negative;
}
int main(void)
{
    const int32_t mixed[] = {-5, 0, 9, -2};
    const int32_t extremes[] = {INT32_MIN, INT32_MAX, INT32_MAX};
    const int32_t positive[] = {4, 5, 6};
    int32_t full[1024];
    for (unsigned i = 0; i < 1024; ++i) full[i] = (int32_t)i - 512;
    int64_t sum = 0;
    uint64_t negative = 0;
    if (!check(NULL, 0) || !check(mixed, 1) || !check(mixed, 4) ||
        !check(extremes, 3) || !check(positive, 3) || !check(full, 1024) ||
        checked_summary(NULL, 1, &sum, &negative) ||
        checked_summary(mixed, 1025, &sum, &negative) ||
        checked_summary(mixed, 4, NULL, &negative) ||
        checked_summary(mixed, 4, &sum, NULL)) return 1;
    if (!checked_summary(mixed, 4, &sum, &negative)) return 1;
    printf("sum: %" PRId64 "\nnegative: %" PRIu64 "\n", sum, negative);
    puts("PASS report: oracle, boundaries and rejected inputs");
    return 0;
}
