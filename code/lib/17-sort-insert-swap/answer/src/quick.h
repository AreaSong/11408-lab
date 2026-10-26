#ifndef QUICK_H
#define QUICK_H

#include "seq.h"

#define Q_LEFT 0
#define Q_RIGHT 1
#define Q_SHORT 2
#define Q_LONG 3

/* 王道划分：枢轴取区间第一个元素。左边都小于等于枢轴，右边都大于等于枢轴。 */
int quick_partition(Seq *s, int low, int high);

/* 一趟处理所有尚未就位的区间，先左后右。这是 2012 年对「一趟」的定义。 */
void quick_trace(Seq *s);
int quick_collect(const Seq *src, Seq *out, int cap);

/* 第 k 趟之后是否可能出现这个序列。枢轴按「左边不大于、右边不小于」检查。 */
int quick_possible(const Seq *s, int passes);

void quick_recur(const Seq *src, int order, long *parts, int *depth);
void quick_opt(const Seq *src, long *parts, int *depth);

#endif
