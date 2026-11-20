#ifndef STATS_H
#define STATS_H

#include "seq.h"

#include <stdio.h>

/* 比较一次关键字记 1。一次记录赋值记 1，所以一次交换是 3。 */
static inline void sort_stats(const Seq *s) {
    printf("cmp=%ld\nmove=%ld\nstable=%s\n", s->cmp, s->move, seq_stable(s) ? "yes" : "no");
}

static inline void sort_sorted_line(const Seq *s) {
    printf("sorted\n");
    seq_print(s);
    sort_stats(s);
}

#endif
