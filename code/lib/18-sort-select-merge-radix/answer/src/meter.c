#include "meter.h"

#include "cost.h"
#include "hsort.h"
#include "merge.h"
#include "quick.h"
#include "select.h"
#include "shell.h"

#include <stdio.h>
#include <string.h>

static void walk(int *key, int n, int pos, void (*fn)(const int *, int, void *), void *ctx) {
    int i;
    if (pos == n) {
        fn(key, n, ctx);
        return;
    }
    for (i = pos; i < n; i++) {
        int t = key[pos];
        key[pos] = key[i];
        key[i] = t;
        walk(key, n, pos + 1, fn, ctx);
        t = key[pos];
        key[pos] = key[i];
        key[i] = t;
    }
}

static void perm_each(int *key, int n, void (*fn)(const int *, int, void *), void *ctx) {
    walk(key, n, 0, fn, ctx);
}

static const char *yesno(int yes) {
    return yes ? "yes" : "no";
}

static void letter_of(char pick) {
    if (pick >= 'A' && pick <= 'D') {
        printf("letter=%c\n", pick);
    } else {
        printf("letter=none\n");
    }
}

typedef struct {
    int select_bad;
    int shell_miss;
    int quick_bad;
    int heap_bad;
    int merge_miss;
} Place;

static void see_place(const int *key, int n, void *ctx) {
    Place *p = ctx;
    Seq s;
    int at;
    seq_load(&s, key, n);
    select_sort(&s, 1, 0);
    if (!seq_final_at(&s, 0)) {
        p->select_bad++;
    }
    seq_load(&s, key, n);
    shell_pass(&s, n / 2);
    if (seq_fixed(&s) == 0) {
        p->shell_miss++;
    }
    seq_load(&s, key, n);
    at = quick_partition(&s, 0, n - 1);
    if (!seq_final_at(&s, at)) {
        p->quick_bad++;
    }
    seq_load(&s, key, n);
    hsort_sort(&s, 1, 0);
    if (!seq_final_at(&s, n - 1)) {
        p->heap_bad++;
    }
    seq_load(&s, key, n);
    merge_sort(&s, 1, 0);
    if (seq_fixed(&s) == 0) {
        p->merge_miss++;
    }
}

static int fixed_of(const int *key, int n, void (*fn)(Seq *)) {
    Seq s;
    seq_load(&s, key, n);
    fn(&s);
    return seq_fixed(&s);
}

static void shell_one(Seq *s) {
    shell_pass(s, s->n / 2);
}

static void merge_one(Seq *s) {
    merge_sort(s, 1, 0);
}

char meter_2012(int printing) {
    int key[4];
    Place p = {0, 0, 0, 0, 0};
    int i;
    int shell_ex[] = {4, 1, 3, 2};
    int merge_ex[] = {4, 2, 3, 1};
    char pick = '?';
    for (i = 0; i < 4; i++) {
        key[i] = i + 1;
    }
    perm_each(key, 4, see_place, &p);
    if (p.select_bad == 0 && p.shell_miss > 0 && p.quick_bad == 0 && p.heap_bad == 0 &&
        p.merge_miss > 0) {
        pick = 'A';
    }
    if (printing) {
        printf("exam 2012-10\n");
        printf("n=4\n");
        printf("select_always=%s\n", yesno(p.select_bad == 0));
        printf("shell_always=%s\n", yesno(p.shell_miss == 0));
        printf("shell_miss=%d\n", p.shell_miss);
        printf("shell_example=4 1 3 2 fixed=%d\n", fixed_of(shell_ex, 4, shell_one));
        printf("quick_always=%s\n", yesno(p.quick_bad == 0));
        printf("heap_always=%s\n", yesno(p.heap_bad == 0));
        printf("merge_always=%s\n", yesno(p.merge_miss == 0));
        printf("merge_miss=%d\n", p.merge_miss);
        printf("merge_example=4 2 3 1 fixed=%d\n", fixed_of(merge_ex, 4, merge_one));
        letter_of(pick);
    }
    return pick;
}

typedef struct {
    long lo[5];
    long hi[5];
    int seen;
} Band;

static void take_band(long *lo, long *hi, long v, int seen) {
    if (!seen || v < *lo) {
        *lo = v;
    }
    if (!seen || v > *hi) {
        *hi = v;
    }
}

static void see_band(const int *key, int n, void *ctx) {
    Band *b = ctx;
    static const char *names[] = {"radix", "insert", "bubble", "quick", "select"};
    int i;
    for (i = 0; i < 5; i++) {
        take_band(&b->lo[i], &b->hi[i], cost_of(names[i], key, n).move, b->seen);
    }
    b->seen = 1;
}

char meter_2015(int printing) {
    int key[] = {12, 21, 13, 31};
    Band b;
    char pick = '?';
    int i;
    memset(&b, 0, sizeof b);
    perm_each(key, 4, see_band, &b);
    if (b.lo[0] == b.hi[0] && b.lo[1] != b.hi[1] && b.lo[2] != b.hi[2] && b.lo[3] != b.hi[3]) {
        pick = 'C';
    }
    if (printing) {
        static const char *names[] = {"radix", "insert", "bubble", "quick", "select"};
        printf("exam 2015-9\n");
        for (i = 0; i < 5; i++) {
            printf("%s_move=%ld..%ld same=%s\n", names[i], b.lo[i], b.hi[i],
                   yesno(b.lo[i] == b.hi[i]));
        }
        letter_of(pick);
    }
    return pick;
}

char meter_2020(int printing) {
    int up[] = {1, 2, 3, 4, 5};
    int down[] = {5, 4, 3, 2, 1};
    Cost ins = cost_of("insert", up, 5);
    Cost sel = cost_of("select", up, 5);
    Cost ins_down = cost_of("insert", down, 5);
    Cost sel_down = cost_of("select", down, 5);
    int fewer_cmp = ins.cmp < sel.cmp;
    int fewer_space = 0;
    int fewer_move = ins_down.move < sel_down.move;
    char pick = '?';
    if (fewer_cmp && !fewer_space && !fewer_move && sel.cmp == sel_down.cmp && ins.move == 0 &&
        sel.move == 0) {
        pick = 'A';
    }
    if (printing) {
        printf("exam 2020-11\n");
        printf("insert_cmp_up=%ld\n", ins.cmp);
        printf("select_cmp_up=%ld\n", sel.cmp);
        printf("select_cmp_down=%ld\n", sel_down.cmp);
        printf("insert_move_up=%ld\n", ins.move);
        printf("select_move_up=%ld\n", sel.move);
        printf("space=1\n");
        printf("insert_move_down=%ld\n", ins_down.move);
        printf("select_move_down=%ld\n", sel_down.move);
        printf("fewer_cmp=%s\n", yesno(fewer_cmp));
        printf("fewer_space=%s\n", yesno(fewer_space));
        printf("fewer_move=%s\n", yesno(fewer_move));
        letter_of(pick);
    }
    return pick;
}

typedef struct {
    long ins;
    long bub;
    long qui;
    long sel;
    int seen;
} Worst;

static void see_worst(const int *key, int n, void *ctx) {
    Worst *w = ctx;
    long ins = cost_of("insert", key, n).move;
    long bub = cost_of("bubble", key, n).move;
    long qui = cost_of("quick", key, n).move;
    long sel = cost_of("select", key, n).move;
    if (!w->seen || ins > w->ins) {
        w->ins = ins;
    }
    if (!w->seen || bub > w->bub) {
        w->bub = bub;
    }
    if (!w->seen || qui > w->qui) {
        w->qui = qui;
    }
    if (!w->seen || sel > w->sel) {
        w->sel = sel;
    }
    w->seen = 1;
}

static void worst_of(int n, Worst *w) {
    int key[12];
    int i;
    memset(w, 0, sizeof *w);
    for (i = 0; i < n; i++) {
        key[i] = i + 1;
    }
    perm_each(key, n, see_worst, w);
}

char meter_2025(int printing) {
    int n;
    int shown = 0;
    Worst w;
    char pick = '?';
    for (n = 5; n <= 8; n++) {
        worst_of(n, &w);
        if (w.sel == 3L * (n - 1) && w.bub > w.sel && w.ins > w.sel && w.qui > w.sel) {
            shown = n;
            pick = 'D';
            break;
        }
    }
    if (shown == 0) {
        shown = 8;
        worst_of(8, &w);
    }
    if (printing) {
        printf("exam 2025-10\n");
        printf("n=%d\n", shown);
        printf("select_worst=%ld\n", w.sel);
        printf("bubble_worst=%ld\n", w.bub);
        printf("insert_worst=%ld\n", w.ins);
        printf("quick_worst=%ld\n", w.qui);
        printf("select_cap=%ld\n", 3L * (shown - 1));
        letter_of(pick);
    }
    return pick;
}

static void show_cost(const char *name, const int *key, int n) {
    Cost c = cost_of(name, key, n);
    printf("%s cmp=%ld move=%ld\n", name, c.cmp, c.move);
}

static void show_row(const char *row, const int *key, int n) {
    static const char *names[] = {"insert", "binary", "shell", "bubble", "quick",
                                  "select", "heap",   "merge", "radix"};
    int i;
    printf("row %s\n", row);
    for (i = 0; i < 9; i++) {
        show_cost(names[i], key, n);
    }
}

void meter_count(void) {
    static const int up[] = {1, 2, 3, 4, 5};
    static const int down[] = {5, 4, 3, 2, 1};
    static const int classic[] = {49, 38, 65, 97, 76, 13, 27, 49};
    show_row("up", up, 5);
    show_row("down", down, 5);
    show_row("classic", classic, 8);
}

void meter_table(void) {
    static const char *name[] = {"insert", "binary", "shell",  "bubble", "quick",
                                 "select", "heap",   "merge",  "radix"};
    static const char *best[] = {"n", "nlogn", "nlogn", "n", "nlogn", "n2", "nlogn", "nlogn", "d(n+r)"};
    static const char *avg[] = {"n2", "n2", "n1.3", "n2", "nlogn", "n2", "nlogn", "nlogn", "d(n+r)"};
    static const char *worst[] = {"n2", "n2", "n2", "n2", "n2", "n2", "nlogn", "nlogn", "d(n+r)"};
    static const char *space[] = {"1", "1", "1", "1", "logn", "1", "1", "n", "r"};
    static const int stable[] = {1, 1, 0, 1, 0, 0, 0, 1, 1};
    int i;
    int ok = 1;
    for (i = 0; i < 9; i++) {
        int live = live_stable(name[i]);
        printf("%s best=%s avg=%s worst=%s space=%s stable=%s live=%s\n", name[i], best[i], avg[i],
               worst[i], space[i], yesno(stable[i]), yesno(live == 1));
        if (live != stable[i]) {
            ok = 0;
        }
    }
    printf("quick_worst_space=n\n");
    printf("radix_array_space=n+r\n");
    printf("live_ok=%s\n", yesno(ok));
}
