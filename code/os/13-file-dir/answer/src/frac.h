#ifndef FRAC_H
#define FRAC_H

/* 平均比较次数用最简分数，避免 5000.5 这种小数在测试里对不齐。 */
typedef struct {
    long long num;
    long long den;
} Frac;

void frac_set(Frac *f, long long num, long long den);

#endif
