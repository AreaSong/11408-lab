#ifndef NUMBER_H
#define NUMBER_H

#include <stddef.h>
#include <stdbool.h>

/* 十进制字符串收成最简分数。num 是绝对值，符号单独放。 */

typedef struct {
    int sign;
    unsigned long long num;
    unsigned long long den;
    bool minus_zero;
} Decimal;

bool decimal_parse(const char *text, Decimal *out, char *err, size_t errn);
bool decimal_is_int(const Decimal *d);
/* den 必须是 1，并且 sign*num 放得进 long long */
bool decimal_to_i64(const Decimal *d, long long *out);

/* fn/fd 写成十进制。12 位以内能除尽就写小数，否则写带分数。 */
bool format_ratio(unsigned long long fn, unsigned long long fd, char *dst, size_t n);

#endif
