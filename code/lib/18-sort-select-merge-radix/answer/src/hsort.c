#include "hsort.h"

#include "heap.h"
#include "stats.h"

#include <stdio.h>

/*
 * 第 09 关的向下筛选是 static，这里按同一条规则再写一遍：
 * 下标从 1 起，右孩子关键字严格大于左孩子才走向右边，相等的孩子留在左边。
 * 父亲不小于选中的孩子就停。建堆结果用 heap_build 核对。
 * 一趟堆排序是把堆顶换到当前堆尾，再对变短的堆筛选。建堆单独算，不算第一趟。
 */

static void swap_rec(Rec *a, int i, int j, long *move) {
    Rec t;
    if (i == j) {
        return;
    }
    t = a[i];
    (*move)++;
    a[i] = a[j];
    (*move)++;
    a[j] = t;
    (*move)++;
}

static int larger_child(const Rec *a, int n, int i, long *cmp) {
    int left = i * 2;
    int right = left + 1;
    if (left > n) {
        return 0;
    }
    if (right > n) {
        return left;
    }
    (*cmp)++;
    if (a[right].key > a[left].key) {
        return right;
    }
    return left;
}

static void sift(Rec *a, int n, int i, long *cmp, long *move) {
    for (;;) {
        int child = larger_child(a, n, i, cmp);
        if (child == 0) {
            return;
        }
        (*cmp)++;
        if (a[i].key >= a[child].key) {
            return;
        }
        swap_rec(a, i, child, move);
        i = child;
    }
}

static void build_heap(Rec *a, int n, long *cmp, long *move) {
    int i;
    for (i = n / 2; i >= 1; i--) {
        sift(a, n, i, cmp, move);
    }
}

static int built_same(const int *raw, int n, const Rec *a) {
    Heap h;
    int i;
    if (heap_build(&h, raw, n, 1, NULL) != HEAP_OK) {
        return 0;
    }
    for (i = 1; i <= n; i++) {
        if (h.key[i] != a[i].key) {
            return 0;
        }
    }
    return 1;
}

static void dump_heap(const Rec *a, int n) {
    int i;
    for (i = 1; i <= n; i++) {
        if (i > 1) {
            putchar(' ');
        }
        printf("%d", a[i].key);
    }
    putchar('\n');
}

static void extract_passes(Rec *a, int n, int limit, long *cmp, long *move, int printing) {
    int end = n;
    int pass;
    for (pass = 1; pass <= limit; pass++) {
        swap_rec(a, 1, end, move);
        end--;
        sift(a, end, 1, cmp, move);
        if (printing) {
            printf("pass %d at=%d key=%d\n", pass, end, a[end + 1].key);
            dump_heap(a, n);
        }
    }
}

static void write_back(Seq *s, const Rec *a) {
    int i;
    for (i = 0; i < s->n; i++) {
        s->a[i] = a[i + 1];
    }
}

int hsort_sort(Seq *s, int max_pass, int printing) {
    Rec a[SORT_MAX + 1];
    int raw[SORT_MAX];
    int n = s->n;
    int limit;
    int same = 1;
    long cmp = 0;
    long move = 0;
    int i;
    if (n <= 1) {
        s->cmp = 0;
        s->move = 0;
        if (printing) {
            sort_sorted_line(s);
        }
        return 1;
    }
    for (i = 0; i < n; i++) {
        a[i + 1] = s->a[i];
        raw[i] = s->a[i].key;
    }
    build_heap(a, n, &cmp, &move);
    same = built_same(raw, n, a);
    if (printing) {
        printf("build\n");
        dump_heap(a, n);
        printf("heap09=%s\n", same ? "yes" : "no");
    }
    limit = n - 1;
    if (max_pass > 0 && max_pass < limit) {
        limit = max_pass;
    }
    extract_passes(a, n, limit, &cmp, &move, printing);
    write_back(s, a);
    s->cmp = cmp;
    s->move = move;
    if (printing) {
        sort_stats(s);
    }
    return same;
}
