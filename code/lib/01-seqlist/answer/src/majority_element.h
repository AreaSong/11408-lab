#ifndef MAJORITY_ELEMENT_H
#define MAJORITY_ELEMENT_H

#include <stdio.h>

/* 2013-41：主元素是出现次数严格大于 n/2 的值，没有则返回 -1。
 * 值域含 0，所以“没有”不能用 0 表示。trace 非 NULL 时打印候选的抵消过程。 */
int majority_element(const int a[], int n, FILE *trace);

#endif
