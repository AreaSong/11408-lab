#ifndef HSORT_H
#define HSORT_H

#include "seq.h"

/* 返回 1 表示建堆后的关键字与第 09 关 heap_build 一致。max_pass 为 0 时排完。 */
int hsort_sort(Seq *s, int max_pass, int printing);

#endif
