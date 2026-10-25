#ifndef MAX_H
#define MAX_H

#include <stdbool.h>

/* 求 a[0..n-1] 的最大值，写入 *out。
 * 空表（n <= 0）没有最大值，返回 false 且不改 *out——
 * 不能用某个"特殊值"表示"没有"，因为任何 int 都可能是合法元素。 */
bool max_find(const int a[], int n, int *out);

#endif
