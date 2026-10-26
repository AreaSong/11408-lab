#ifndef ASL_H
#define ASL_H

#include <stdio.h>

/* 成功、失败各一个分数。分子分母都不在这里约分，打印时再约。 */
typedef struct {
    long long succ_n;
    long long succ_d;
    long long fail_n;
    long long fail_d;
} AslPair;

/* 写成最简分数，再给一个四舍五入到两位的小数。做题以分数为准。 */
void asl_write(FILE *out, long long num, long long den);
int asl_same(long long n1, long long d1, long long n2, long long d2);
int asl_less(long long n1, long long d1, long long n2, long long d2);

#endif
