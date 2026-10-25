#ifndef TOPK_H
#define TOPK_H

#include <stdio.h>

/*
 * 用大小为 k 的小根堆保留最大的 k 个数。
 * 堆顶是这 k 个里最小的：新数不大于堆顶就进不了前 k 名；更大就换掉堆顶再向下筛选。
 */
int topk_run(const int *keys, int n, int k, FILE *out);

#endif
