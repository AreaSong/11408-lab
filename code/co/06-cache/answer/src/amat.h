#ifndef AMAT_H
#define AMAT_H

#include <stdio.h>

/*
 * 平均访问时间的分子分母，已经约分。
 * 先访问 Cache：seq。Cache 与主存同时访问：par。效率 e = tc / t。
 */
typedef struct {
    long long seq_num;
    long long seq_den;
    long long par_num;
    long long par_den;
    long long seq_e_num;
    long long seq_e_den;
    long long par_e_num;
    long long par_e_den;
} Amat;

/* tc、tm 的单位是 ns。次数不合法时返回 1，不打印。 */
int amat_make(Amat *out, long long tc, long long tm, long long accesses, long long misses);

int show_amat(FILE *out, long long tc, long long tm, long long accesses, long long misses);

#endif
