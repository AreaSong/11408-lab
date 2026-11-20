#ifndef MERGE_H
#define MERGE_H

#include "seq.h"

/* 两段有序表合并。相等取左边，剩余直接抄过去，不再比较。返回写出的个数。 */
int merge_pair(const int *a, int na, const int *b, int nb, int *out, long *cmp);

/* 从左到右把若干有序段两两收成一段。返回最终长度，失败返回 −1。 */
int merge_chain(const int *flat, const int *len, int nr, int *out, long *cmp, int printing);

/* 自底向上二路归并。max_pass 为 0 时做到整表有序。相等关键字先取左段。 */
void merge_sort(Seq *s, int max_pass, int printing);

#endif
