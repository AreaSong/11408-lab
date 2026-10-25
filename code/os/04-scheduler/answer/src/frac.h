#ifndef FRAC_H
#define FRAC_H

#include <stddef.h>

/* 周转时间除以进程个数、带权周转都用最简分数，避免 5/3 和 3/2 被浮点比错。 */
typedef struct {
    long long n;
    long long d;
} Frac;

Frac frac_make(long long n, long long d);
int frac_add(Frac *acc, Frac b);
int frac_div_int(Frac a, long long k, Frac *out);
int frac_fmt(char *buf, size_t n, Frac f);

#endif
