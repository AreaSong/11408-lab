#include "judge.h"

#include "bubble.h"
#include "insert.h"
#include "quick.h"
#include "shell.h"

#include <stdio.h>

static void yn(const char *name, int yes) {
    printf("%s=%s\n", name, yes ? "yes" : "no");
}

static void show_side(const Seq *s, int lo, int hi, const char *tag) {
    int i;
    printf("%s", tag);
    if (lo > hi) {
        printf(" -\n");
        return;
    }
    for (i = lo; i <= hi; i++) {
        printf(" %d", s->a[i].key);
    }
    putchar('\n');
}

static int has_dup(const Seq *s, int lo, int hi) {
    int i;
    int j;
    for (i = lo; i <= hi; i++) {
        for (j = i + 1; j <= hi; j++) {
            if (s->a[i].key == s->a[j].key) {
                return 1;
            }
        }
    }
    return 0;
}

static int side_sorted(const Seq *s, int lo, int hi) {
    if (lo >= hi) {
        return 1;
    }
    return seq_ordered(s, lo, hi);
}

static int partition_copy(const int *key, int n, Seq *out, int *at) {
    Seq s;
    seq_load(&s, key, n);
    *at = quick_partition(&s, 0, n - 1);
    *out = s;
    return seq_final_at(&s, *at);
}

static void merge_range(Rec *a, Rec *tmp, int lo, int mid, int hi) {
    int i = lo;
    int j = mid;
    int k = lo;
    while (i < mid && j < hi) {
        if (a[i].key <= a[j].key) {
            tmp[k++] = a[i++];
        } else {
            tmp[k++] = a[j++];
        }
    }
    while (i < mid) {
        tmp[k++] = a[i++];
    }
    while (j < hi) {
        tmp[k++] = a[j++];
    }
    for (i = lo; i < hi; i++) {
        a[i] = tmp[i];
    }
}

/* 只做自底向上的一趟。完整归并排序在第 18 关。相等时先取左段，所以这一趟是稳定的。 */
static void merge_one(Seq *s, int width) {
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
        merge_range(s->a, tmp, i, mid, hi);
    }
}

static int merge_run(const Seq *init, Seq *out, int np) {
    Seq s = *init;
    int width = 1;
    int i;
    for (i = 0; i < np; i++) {
        merge_one(&s, width);
        out[i] = s;
        if (width > SORT_MAX) {
            break;
        }
        width *= 2;
    }
    return i;
}

/* 最低位优先的一趟分配收集。负数没有个位这种切法，返回 0。 */
static int radix_one(Seq *s, int exp) {
    int count[10] = {0};
    Rec out[SORT_MAX];
    int i;
    int d;
    for (i = 0; i < s->n; i++) {
        if (s->a[i].key < 0) {
            return 0;
        }
        d = (s->a[i].key / exp) % 10;
        count[d]++;
    }
    for (d = 1; d < 10; d++) {
        count[d] += count[d - 1];
    }
    for (i = s->n - 1; i >= 0; i--) {
        d = (s->a[i].key / exp) % 10;
        count[d]--;
        out[count[d]] = s->a[i];
    }
    for (i = 0; i < s->n; i++) {
        s->a[i] = out[i];
    }
    return 1;
}

static int radix_run(const Seq *init, Seq *out, int np) {
    Seq s = *init;
    int exp = 1;
    int i;
    for (i = 0; i < np; i++) {
        if (!radix_one(&s, exp)) {
            return 0;
        }
        out[i] = s;
        exp *= 10;
    }
    return 1;
}

static int passes_match(const Seq *got, int ng, const Seq *want, int nw) {
    int i;
    if (ng < nw) {
        return 0;
    }
    for (i = 0; i < nw; i++) {
        if (!seq_same(&got[i], &want[i])) {
            return 0;
        }
    }
    return 1;
}

static void print_gaps(const int *gaps, int n) {
    int i;
    printf("gaps");
    for (i = 0; i < n; i++) {
        printf(" %d", gaps[i]);
    }
    putchar('\n');
}

void judge_family(int year, const Seq *init, const Seq *pass, int np) {
    Seq got[SORT_MAX];
    Seq bin_got[SORT_MAX];
    Seq rise_got[SORT_MAX];
    Seq sink_got[SORT_MAX];
    Seq quick_got[SORT_MAX];
    Seq merge_got[SORT_MAX];
    Seq radix_got[SORT_MAX];
    int gaps[4];
    int nins = insert_collect(init, got, SORT_MAX, 0);
    int nbin = insert_collect(init, bin_got, SORT_MAX, 1);
    int nrise = bubble_collect(init, rise_got, SORT_MAX, 0);
    int nsink = bubble_collect(init, sink_got, SORT_MAX, 1);
    int nquick = quick_collect(init, quick_got, SORT_MAX);
    int ins;
    int bin;
    int rise;
    int sink;
    int shell;
    int quick;
    int merge;
    int radix;
    ins = passes_match(got, nins, pass, np);
    bin = passes_match(bin_got, nbin, pass, np);
    rise = passes_match(rise_got, nrise, pass, np);
    sink = passes_match(sink_got, nsink, pass, np);
    shell = shell_match(init, pass, np, gaps);
    quick = passes_match(quick_got, nquick, pass, np);
    merge = passes_match(merge_got, merge_run(init, merge_got, np), pass, np);
    radix = radix_run(init, radix_got, np) && passes_match(radix_got, np, pass, np);
    yn("insert", ins);
    yn("binary", bin);
    yn("shell", shell);
    if (shell) {
        print_gaps(gaps, np);
    }
    yn("rise", rise);
    yn("sink", sink);
    yn("quick", quick);
    yn("merge", merge);
    yn("radix", radix);
    if (year == 2010 && (rise || sink) && !shell && !merge && !radix) {
        printf("letter=A\n");
    } else if (year == 2025 && shell && !bin && !merge && !radix) {
        printf("letter=A\n");
    } else {
        printf("letter=none\n");
    }
}

void judge_half(const Seq *s) {
    Seq direct[SORT_MAX];
    Seq binary[SORT_MAX];
    int nd = insert_collect(s, direct, SORT_MAX, 0);
    int nb = insert_collect(s, binary, SORT_MAX, 1);
    long md = nd == 0 ? 0 : direct[nd - 1].move;
    long mb = nb == 0 ? 0 : binary[nb - 1].move;
    long cd = nd == 0 ? 0 : direct[nd - 1].cmp;
    long cb = nb == 0 ? 0 : binary[nb - 1].cmp;
    int same_pass = nd == nb;
    int same_move = md == mb;
    int same_cmp = cd == cb;
    printf("passes=%d\n", nd);
    printf("move_insert=%ld\nmove_binary=%ld\n", md, mb);
    printf("cmp_insert=%ld\ncmp_binary=%ld\n", cd, cb);
    printf("space=1\n");
    yn("passes_same", same_pass);
    yn("moves_same", same_move);
    yn("space_same", 1);
    yn("cmps_same", same_cmp);
    if (same_pass && same_move && !same_cmp) {
        printf("letter=D\n");
    } else {
        printf("letter=none\n");
    }
}

static void report_orders(const char *title, const int *key, int n) {
    Seq s;
    long parts[4];
    int depth[4];
    long opt_parts;
    int opt_depth;
    int i;
    seq_load(&s, key, n);
    printf("%s", title);
    seq_print(&s);
    for (i = 0; i < 4; i++) {
        quick_recur(&s, i, &parts[i], &depth[i]);
    }
    quick_opt(&s, &opt_parts, &opt_depth);
    printf("parts %ld %ld %ld %ld\n", parts[0], parts[1], parts[2], parts[3]);
    printf("depth %d %d %d %d\n", depth[0], depth[1], depth[2], depth[3]);
    printf("opt_parts=%ld\nopt_depth=%d\n", opt_parts, opt_depth);
}

void judge_calls(void) {
    int sorted[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int mixed[] = {49, 38, 65, 97, 76, 13, 27, 49};
    Seq a;
    Seq b;
    long pa[4];
    long pb[4];
    int da[4];
    int db[4];
    int i;
    int order_same = 1;
    int data_same;
    report_orders("sorted ", sorted, 8);
    report_orders("mixed ", mixed, 8);
    seq_load(&a, sorted, 8);
    seq_load(&b, mixed, 8);
    for (i = 0; i < 4; i++) {
        quick_recur(&a, i, &pa[i], &da[i]);
        quick_recur(&b, i, &pb[i], &db[i]);
        if (pa[i] != pa[0] || da[i] != da[0] || pb[i] != pb[0] || db[i] != db[0]) {
            order_same = 0;
        }
    }
    data_same = pa[0] == pb[0];
    yn("order_same", order_same);
    yn("data_same", data_same);
    if (order_same && !data_same) {
        printf("letter=D\n");
    } else {
        printf("letter=none\n");
    }
}

static void select_one(Seq *s) {
    int i;
    int m = 0;
    for (i = 1; i < s->n; i++) {
        if (s->a[i].key < s->a[m].key) {
            m = i;
        }
    }
    seq_swap(s, 0, m);
}

void judge_fixes(void) {
    int shell_key[] = {4, 1, 3, 2};
    int merge_key[] = {4, 3, 2, 1};
    int select_key[] = {3, 1, 4, 2};
    int quick_key[] = {49, 38, 65, 97, 76, 13, 27, 49};
    Seq s;
    int at;
    seq_load(&s, shell_key, 4);
    shell_pass(&s, 2);
    printf("shell_out ");
    seq_print(&s);
    printf("shell_fixed=%d\n", seq_fixed(&s));
    yn("shell", seq_fixed(&s) > 0);
    seq_load(&s, merge_key, 4);
    merge_one(&s, 1);
    printf("merge_out ");
    seq_print(&s);
    printf("merge_fixed=%d\n", seq_fixed(&s));
    yn("merge", seq_fixed(&s) > 0);
    seq_load(&s, select_key, 4);
    select_one(&s);
    printf("select_out ");
    seq_print(&s);
    printf("select_fixed=%d\n", seq_fixed(&s));
    yn("select", seq_final_at(&s, 0));
    partition_copy(quick_key, 8, &s, &at);
    printf("quick_out ");
    seq_print(&s);
    printf("quick_at=%d\nquick_fixed=%d\n", at, seq_fixed(&s));
    yn("quick", seq_final_at(&s, at));
    /* 堆排序每趟把当前最大换到末尾，过程在第 18 关。 */
    yn("heap", 1);
    yn("I", 1);
    yn("II", 0);
    yn("III", 1);
    yn("IV", 1);
    yn("V", 0);
    printf("letter=A\n");
}

static void dump_block(const char *title, const int *key, int n) {
    Seq s;
    int at;
    partition_copy(key, n, &s, &at);
    printf("%s ", title);
    seq_print(&s);
    show_side(&s, 0, at - 1, "P");
    printf("pivot %d\n", s.a[at].key);
    show_side(&s, at + 1, s.n - 1, "Q");
    printf("P_len=%d\nQ_len=%d\n", at, s.n - at - 1);
    yn("inside", side_sorted(&s, 0, at - 1) && side_sorted(&s, at + 1, s.n - 1));
    yn("dup", has_dup(&s, 0, at - 1) || has_dup(&s, at + 1, s.n - 1));
    yn("between", seq_final_at(&s, at));
}

void judge_blocks(void) {
    int shape[] = {49, 38, 65, 97, 76, 13, 27, 49};
    int span[] = {2, 1, 3, 4, 5, 6, 7, 8};
    int dup[] = {3, 5, 5, 1, 4, 2};
    dump_block("shape", shape, 8);
    dump_block("span", span, 8);
    dump_block("dup", dup, 6);
    /* 块间有序是划分的后置条件；块内有序、长度接近、没有相等元素都有反例。 */
    printf("letter=A\n");
}

static void finish_insert(Seq *s) {
    int i;
    for (i = 1; i < s->n; i++) {
        gap_insert(s, i, 1);
    }
}

void judge_stable(void) {
    int shell_key[] = {2, 2, 1};
    int plain[] = {3, 3, 2};
    int quick_key[] = {3, 3, 2};
    Seq s;
    Seq out[SORT_MAX];
    int n;
    seq_load(&s, shell_key, 3);
    shell_pass(&s, 2);
    shell_pass(&s, 1);
    printf("shell_out ");
    seq_print_ord(&s);
    yn("shell_stable", seq_stable(&s));
    seq_load(&s, plain, 3);
    finish_insert(&s);
    printf("insert_out ");
    seq_print_ord(&s);
    yn("insert_stable", seq_stable(&s));
    seq_load(&s, plain, 3);
    n = insert_collect(&s, out, SORT_MAX, 1);
    printf("binary_out ");
    seq_print_ord(n == 0 ? &s : &out[n - 1]);
    yn("binary_stable", seq_stable(n == 0 ? &s : &out[n - 1]));
    seq_load(&s, plain, 3);
    n = bubble_collect(&s, out, SORT_MAX, 0);
    printf("rise_out ");
    seq_print_ord(&out[n - 1]);
    yn("rise_stable", seq_stable(&out[n - 1]));
    seq_load(&s, plain, 3);
    n = bubble_collect(&s, out, SORT_MAX, 1);
    printf("sink_out ");
    seq_print_ord(&out[n - 1]);
    yn("sink_stable", seq_stable(&out[n - 1]));
    seq_load(&s, quick_key, 3);
    n = quick_collect(&s, out, SORT_MAX);
    printf("quick_out ");
    seq_print_ord(&out[n - 1]);
    yn("quick_stable", seq_stable(&out[n - 1]));
    /* 归并、堆、基数的完整过程在第 18 关。归并和基数按稳定的方式合并；堆交换不看相等。 */
    yn("merge_stable", 1);
    yn("heap_stable", 0);
    yn("radix_stable", 1);
    printf("letter=C\n");
}
