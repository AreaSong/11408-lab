#ifndef MISSMIN_H
#define MISSMIN_H

/* 2018-41：未出现的最小正整数。标记数组写法，时间 O(n)、空间 O(n)。
 * n < 1 时答案是 1；分配失败返回 -1。 */
int find_miss_min(const int *a, int n);

#endif
