#ifndef RADIX_H
#define RADIX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    int sign;
    unsigned long long ip;
    unsigned long long fn;
    unsigned long long fd;
} RadixParts;

bool radix_base_ok(int base);
const char *radix_name(int base);
int radix_parse(int base, const char *text, RadixParts *out, char *err, size_t errn);

/* exact 为假表示小数在 frac_digits 位内没有除尽。成功返回 0。 */
int radix_result(int from, int to, int frac_digits, const char *text, char *dst, size_t n, bool *exact,
                 char *err, size_t errn);
int radix_print(FILE *out, int from, int to, int frac_digits, const char *text);

#endif
