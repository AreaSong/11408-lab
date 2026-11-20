#include "merge.h"

#include "stats.h"

#include <stdio.h>
#include <string.h>

static void print_ints(const int *a, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (i > 0) {
            putchar(' ');
        }
        printf("%d", a[i]);
    }
    putchar('\n');
}

int merge_pair(const int *a, int na, const int *b, int nb, int *out, long *cmp) {
    int i = 0;
    int j = 0;
    int k = 0;
    *cmp = 0;
    while (i < na && j < nb) {
        (*cmp)++;
        if (a[i] <= b[j]) {
            out[k++] = a[i++];
        } else {
            out[k++] = b[j++];
        }
    }
    while (i < na) {
        out[k++] = a[i++];
    }
    while (j < nb) {
        out[k++] = b[j++];
    }
    return k;
}

int merge_chain(const int *flat, const int *len, int nr, int *out, long *cmp, int printing) {
    int cur[SORT_MAX];
    int ncur;
    int offset;
    int r;
    long total = 0;
    if (nr < 1 || len[0] < 1 || len[0] > SORT_MAX) {
        return -1;
    }
    ncur = len[0];
    memcpy(cur, flat, (size_t)ncur * sizeof(int));
    offset = ncur;
    for (r = 1; r < nr; r++) {
        int nxt[SORT_MAX];
        long step = 0;
        int got;
        if (len[r] < 1 || ncur + len[r] > SORT_MAX) {
            return -1;
        }
        got = merge_pair(cur, ncur, flat + offset, len[r], nxt, &step);
        memcpy(cur, nxt, (size_t)got * sizeof(int));
        ncur = got;
        offset += len[r];
        total += step;
        if (printing) {
            printf("step %d cmp=%ld\n", r, step);
            print_ints(cur, ncur);
        }
    }
    if (printing && nr == 1) {
        print_ints(cur, ncur);
    }
    if (printing) {
        printf("cmp=%ld\n", total);
    }
    if (out != NULL) {
        memcpy(out, cur, (size_t)ncur * sizeof(int));
    }
    if (cmp != NULL) {
        *cmp = total;
    }
    return ncur;
}

/* 写入辅助数组再写回，所以一段长度为 L 的归并移动 2L 次，与关键字次序无关。 */
static void merge_span(Seq *s, Rec *tmp, int lo, int mid, int hi) {
    int i = lo;
    int j = mid;
    int k = lo;
    while (i < mid && j < hi) {
        s->cmp++;
        if (s->a[i].key <= s->a[j].key) {
            tmp[k++] = s->a[i++];
        } else {
            tmp[k++] = s->a[j++];
        }
        s->move++;
    }
    while (i < mid) {
        tmp[k++] = s->a[i++];
        s->move++;
    }
    while (j < hi) {
        tmp[k++] = s->a[j++];
        s->move++;
    }
    for (i = lo; i < hi; i++) {
        s->a[i] = tmp[i];
        s->move++;
    }
}

static void merge_width(Seq *s, int width) {
    Rec tmp[SORT_MAX];
    int i;
    for (i = 0; i < s->n; i += 2 * width) {
        int mid = i + width;
        int hi = mid + width;
        if (mid >= s->n) {
            break;
        }
        if (hi > s->n) {
            hi = s->n;
        }
        merge_span(s, tmp, i, mid, hi);
    }
}

void merge_sort(Seq *s, int max_pass, int printing) {
    int width = 1;
    int pass = 0;
    s->cmp = 0;
    s->move = 0;
    if (s->n <= 1) {
        if (printing) {
            sort_sorted_line(s);
        }
        return;
    }
    while (width < s->n) {
        if (max_pass > 0 && pass >= max_pass) {
            break;
        }
        merge_width(s, width);
        pass++;
        if (printing) {
            printf("pass %d width=%d\n", pass, width);
            seq_print(s);
        }
        if (width > SORT_MAX / 2) {
            break;
        }
        width *= 2;
    }
    if (printing) {
        sort_stats(s);
    }
}
