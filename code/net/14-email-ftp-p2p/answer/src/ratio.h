#ifndef RATIO_H
#define RATIO_H

#include <stddef.h>

/* 分发时间经常除不尽，用约分后的秒数，避免把 1000/11 s 收成双精度。 */
typedef struct {
    long long num;
    long long den;
} Ratio;

int ratio_make(long long num, long long den, Ratio *out);
int ratio_cmp(Ratio a, Ratio b);
void ratio_fmt(char *buf, size_t n, Ratio t);

#endif
