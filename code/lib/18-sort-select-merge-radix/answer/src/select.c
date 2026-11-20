#include "select.h"

#include "stats.h"

#include <stdio.h>

/*
 * 严格小于才更新最小下标，相等时留在原来的位置。
 * 不稳定来自交换：3a 3b 2 会把 3a 换到 3b 后面。
 * 比较次数只由下标决定，任何次序都是 n(n−1)/2。
 * 每一趟最多交换一次，移动次数最多 3(n−1)。
 */
void select_sort(Seq *s, int max_pass, int printing) {
    int i;
    int limit;
    s->cmp = 0;
    s->move = 0;
    if (s->n <= 1) {
        if (printing) {
            sort_sorted_line(s);
        }
        return;
    }
    limit = s->n - 1;
    if (max_pass > 0 && max_pass < limit) {
        limit = max_pass;
    }
    for (i = 0; i < limit; i++) {
        int m = i;
        int j;
        for (j = i + 1; j < s->n; j++) {
            s->cmp++;
            if (s->a[j].key < s->a[m].key) {
                m = j;
            }
        }
        if (m != i) {
            seq_swap(s, i, m);
        }
        if (printing) {
            printf("pass %d at=%d\n", i + 1, i);
            seq_print(s);
        }
    }
    if (printing) {
        sort_stats(s);
    }
}
