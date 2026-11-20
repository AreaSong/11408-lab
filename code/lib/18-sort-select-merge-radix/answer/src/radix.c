#include "radix.h"

#include "stats.h"

#include <stdio.h>

/* 从后往前按计数落位，和队列先进先出一样：后扫到的先占桶的右端，相等关键字保持原次序。 */
static void one_pass(Seq *s, int span) {
    int count[10] = {0};
    Rec out[SORT_MAX];
    int i;
    int d;
    for (i = 0; i < s->n; i++) {
        d = (s->a[i].key / span) % 10;
        count[d]++;
    }
    for (d = 1; d < 10; d++) {
        count[d] += count[d - 1];
    }
    for (i = s->n - 1; i >= 0; i--) {
        d = (s->a[i].key / span) % 10;
        count[d]--;
        out[count[d]] = s->a[i];
        s->move++;
    }
    for (i = 0; i < s->n; i++) {
        s->a[i] = out[i];
        s->move++;
    }
}

static int max_key(const Seq *s) {
    int guard = 0;
    int i;
    for (i = 0; i < s->n; i++) {
        if (s->a[i].key < 0) {
            return -1;
        }
        if (s->a[i].key > guard) {
            guard = s->a[i].key;
        }
    }
    return guard;
}

int radix_sort(Seq *s, int max_pass, int printing) {
    int guard = max_key(s);
    int span = 1;
    int step = 0;
    if (guard < 0) {
        return -1;
    }
    s->cmp = 0;
    s->move = 0;
    if (s->n <= 1) {
        if (printing) {
            sort_sorted_line(s);
        }
        return 0;
    }
    while (step < 10) {
        if (max_pass > 0 && step >= max_pass) {
            break;
        }
        one_pass(s, span);
        step++;
        if (printing) {
            printf("pass %d digit=%d\n", step, span);
            seq_print(s);
        }
        if (guard / 10 < span) {
            break;
        }
        span *= 10;
    }
    if (printing) {
        sort_stats(s);
    }
    return 0;
}
