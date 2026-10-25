#ifndef TWO_MEDIAN_H
#define TWO_MEDIAN_H

#include <stdio.h>

/* 2011-42：两个等长升序序列的中位数。
 * 合并后长度为 2n，中位数是第 n 个（从 1 数，即较小的那个，不是两个的平均值）。
 * trace 非 NULL 时按轮打印折半后保留的区间；题目保证 n ≥ 1 且两段已升序。 */
int two_median(const int a[], const int b[], int n, FILE *trace);

#endif
