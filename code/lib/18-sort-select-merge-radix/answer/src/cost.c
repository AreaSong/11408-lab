#include "cost.h"

#include "bubble.h"
#include "hsort.h"
#include "insert.h"
#include "merge.h"
#include "quick.h"
#include "radix.h"
#include "select.h"
#include "shell.h"

#include <stdio.h>
#include <string.h>

static Cost from_collect(int np, const Seq *out) {
    Cost c;
    if (np <= 0) {
        c.cmp = 0;
        c.move = 0;
        return c;
    }
    c.cmp = out[np - 1].cmp;
    c.move = out[np - 1].move;
    return c;
}

static Cost of_insert(const int *key, int n, int binary) {
    Seq s;
    Seq out[SORT_MAX];
    seq_load(&s, key, n);
    return from_collect(insert_collect(&s, out, SORT_MAX, binary), out);
}

static Cost of_shell(const int *key, int n) {
    Seq s;
    int gaps[SORT_MAX];
    int ng;
    int i;
    Cost c;
    seq_load(&s, key, n);
    ng = shell_halve(n, gaps);
    for (i = 0; i < ng; i++) {
        shell_pass(&s, gaps[i]);
    }
    c.cmp = s.cmp;
    c.move = s.move;
    return c;
}

static Cost of_bubble(const int *key, int n) {
    Seq s;
    Seq out[SORT_MAX];
    seq_load(&s, key, n);
    return from_collect(bubble_collect(&s, out, SORT_MAX, 1), out);
}

static Cost of_quick(const int *key, int n) {
    Seq s;
    Seq out[SORT_MAX];
    seq_load(&s, key, n);
    return from_collect(quick_collect(&s, out, SORT_MAX), out);
}

static Cost of_select(const int *key, int n) {
    Seq s;
    Cost c;
    seq_load(&s, key, n);
    select_sort(&s, 0, 0);
    c.cmp = s.cmp;
    c.move = s.move;
    return c;
}

static Cost of_heap(const int *key, int n) {
    Seq s;
    Cost c;
    seq_load(&s, key, n);
    hsort_sort(&s, 0, 0);
    c.cmp = s.cmp;
    c.move = s.move;
    return c;
}

static Cost of_merge(const int *key, int n) {
    Seq s;
    Cost c;
    seq_load(&s, key, n);
    merge_sort(&s, 0, 0);
    c.cmp = s.cmp;
    c.move = s.move;
    return c;
}

static Cost of_radix(const int *key, int n) {
    Seq s;
    Cost c;
    seq_load(&s, key, n);
    if (radix_sort(&s, 0, 0) != 0) {
        c.cmp = -1;
        c.move = -1;
        return c;
    }
    c.cmp = s.cmp;
    c.move = s.move;
    return c;
}

Cost cost_of(const char *name, const int *key, int n) {
    Cost bad;
    bad.cmp = -1;
    bad.move = -1;
    if (strcmp(name, "insert") == 0) {
        return of_insert(key, n, 0);
    }
    if (strcmp(name, "binary") == 0) {
        return of_insert(key, n, 1);
    }
    if (strcmp(name, "shell") == 0) {
        return of_shell(key, n);
    }
    if (strcmp(name, "bubble") == 0) {
        return of_bubble(key, n);
    }
    if (strcmp(name, "quick") == 0) {
        return of_quick(key, n);
    }
    if (strcmp(name, "select") == 0) {
        return of_select(key, n);
    }
    if (strcmp(name, "heap") == 0) {
        return of_heap(key, n);
    }
    if (strcmp(name, "merge") == 0) {
        return of_merge(key, n);
    }
    if (strcmp(name, "radix") == 0) {
        return of_radix(key, n);
    }
    return bad;
}

static void put_back(Seq *s, const Seq *out, int np) {
    if (np > 0) {
        *s = out[np - 1];
    }
}

static void do_insert(Seq *s) {
    Seq out[SORT_MAX];
    put_back(s, out, insert_collect(s, out, SORT_MAX, 0));
}

static void do_binary(Seq *s) {
    Seq out[SORT_MAX];
    put_back(s, out, insert_collect(s, out, SORT_MAX, 1));
}

/* n=3 时折半增量只有 1，组内就是直接插入，看不出跨越。步长 2 才会把相等关键字换位。 */
static void do_shell_gap(Seq *s) {
    shell_pass(s, 2);
}

static void do_bubble(Seq *s) {
    Seq out[SORT_MAX];
    put_back(s, out, bubble_collect(s, out, SORT_MAX, 1));
}

static void do_quick(Seq *s) {
    Seq out[SORT_MAX];
    put_back(s, out, quick_collect(s, out, SORT_MAX));
}

static void do_select(Seq *s) {
    select_sort(s, 0, 0);
}

static void do_heap(Seq *s) {
    hsort_sort(s, 0, 0);
}

static void do_merge(Seq *s) {
    merge_sort(s, 0, 0);
}

static void do_radix(Seq *s) {
    radix_sort(s, 0, 0);
}

static int pick_tail(const char *name, const int **key, void (**fn)(Seq *), const int *heap_key,
                     const int *radix_key);

static int pick_witness(const char *name, const int **key, void (**fn)(Seq *)) {
    static const int pair[] = {3, 3, 2};
    static const int shell_key[] = {2, 2, 1};
    static const int quick_key[] = {3, 3, 1};
    /* 1a 1b 2 在堆只剩两个相等关键字时仍会交换，最终变成 1b 1a 2。 */
    static const int heap_key[] = {1, 1, 2};
    static const int radix_key[] = {21, 11, 21};
    if (strcmp(name, "insert") == 0) {
        *key = pair;
        *fn = do_insert;
        return 3;
    }
    if (strcmp(name, "binary") == 0) {
        *key = pair;
        *fn = do_binary;
        return 3;
    }
    if (strcmp(name, "shell") == 0) {
        *key = shell_key;
        *fn = do_shell_gap;
        return 3;
    }
    if (strcmp(name, "bubble") == 0) {
        *key = pair;
        *fn = do_bubble;
        return 3;
    }
    if (strcmp(name, "quick") == 0) {
        *key = quick_key;
        *fn = do_quick;
        return 3;
    }
    if (strcmp(name, "select") == 0) {
        *key = pair;
        *fn = do_select;
        return 3;
    }
    return pick_tail(name, key, fn, heap_key, radix_key);
}

static int pick_tail(const char *name, const int **key, void (**fn)(Seq *), const int *heap_key,
                     const int *radix_key) {
    if (strcmp(name, "heap") == 0) {
        *key = heap_key;
        *fn = do_heap;
        return 3;
    }
    if (strcmp(name, "merge") == 0) {
        *key = heap_key;
        *fn = do_merge;
        return 3;
    }
    if (strcmp(name, "radix") == 0) {
        *key = radix_key;
        *fn = do_radix;
        return 3;
    }
    return 0;
}

static int run_stable(const char *name, int printing) {
    const int *key = NULL;
    void (*fn)(Seq *) = NULL;
    int n = pick_witness(name, &key, &fn);
    Seq s;
    if (n == 0) {
        return -1;
    }
    seq_load(&s, key, n);
    fn(&s);
    if (printing && strcmp(name, "shell") == 0) {
        printf("gap=2\n");
    }
    if (printing) {
        printf("%s ", name);
        seq_print_ord(&s);
        printf("stable=%s\n", seq_stable(&s) ? "yes" : "no");
    }
    return seq_stable(&s);
}

int live_stable(const char *name) {
    return run_stable(name, 0);
}

void stable_show(void) {
    static const char *names[] = {"insert", "binary", "shell", "bubble", "quick",
                                  "select", "heap",   "merge", "radix"};
    int i;
    for (i = 0; i < 9; i++) {
        run_stable(names[i], 1);
    }
}
