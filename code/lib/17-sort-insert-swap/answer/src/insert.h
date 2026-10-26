#ifndef INSERT_H
#define INSERT_H

#include "seq.h"

/* 步长 d 的一次插入。直接插入是 d = 1，希尔排序复用它。相等关键字不后移。 */
void gap_insert(Seq *s, int i, int d);

/* binary 为 0 是直接插入，为 1 是折半插入。返回实际打出的趟数。 */
int insert_trace(Seq *s, int binary);
int insert_collect(const Seq *src, Seq *out, int cap, int binary);

#endif
