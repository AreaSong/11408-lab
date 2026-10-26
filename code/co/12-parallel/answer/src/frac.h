#ifndef PARALLEL_FRAC_H
#define PARALLEL_FRAC_H

#include <stdio.h>

/* 约成最简分数。分母保持为正。 */
void frac_reduce(long long num, long long den, long long *out_num, long long *out_den);

/* 分母为 1 时只写整数，否则写 a/b。 */
void frac_write(FILE *fp, long long num, long long den);

#endif
