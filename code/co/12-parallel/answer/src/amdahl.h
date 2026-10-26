#ifndef PARALLEL_AMDAHL_H
#define PARALLEL_AMDAHL_H

#include <stdio.h>

/*
 * 多核版 Amdahl。pct 是可并行部分的百分比（80 表示 80%），n 是核数。
 * 加速比分子 / 分母 = (100×n) / (n×(100−pct) + pct)，再约分。
 * 效率 = 加速比 / n。没有串行部分时极限加速比不存在上界。
 */
typedef struct {
    long long speed_num;
    long long speed_den;
    long long eff_num;
    long long eff_den;
    int serial_pct;
    int limit_infinite;
    long long limit_num;
    long long limit_den;
} Amdahl;

Amdahl amdahl_of(int pct, int n);
void amdahl_show(FILE *fp, int pct, int n);
void amdahl_table(FILE *fp, int pct);

#endif
