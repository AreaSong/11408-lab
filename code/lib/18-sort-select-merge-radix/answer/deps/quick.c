#include "quick.h"

#include <stdio.h>

static int scan_down(Seq *s, int low, int high, int pivot) {
    while (low < high) {
        s->cmp++;
        if (s->a[high].key < pivot) {
            return high;
        }
        high--;
    }
    return high;
}

static int scan_up(Seq *s, int low, int high, int pivot) {
    while (low < high) {
        s->cmp++;
        if (s->a[low].key > pivot) {
            return low;
        }
        low++;
    }
    return low;
}

static void store(Seq *s, int dst, int src) {
    if (dst == src) {
        return;
    }
    s->a[dst] = s->a[src];
    s->move++;
}

int quick_partition(Seq *s, int low, int high) {
    Rec pivot = s->a[low];
    s->move++;
    while (low < high) {
        high = scan_down(s, low, high, pivot.key);
        store(s, low, high);
        low = scan_up(s, low, high, pivot.key);
        store(s, high, low);
    }
    s->a[low] = pivot;
    s->move++;
    return low;
}

typedef struct {
    int lo;
    int hi;
} Span;

static int push_span(Span *q, int n, int lo, int hi) {
    if (hi - lo < 1) {
        return n;
    }
    q[n].lo = lo;
    q[n].hi = hi;
    return n + 1;
}

static int one_pass(Seq *s, Span *cur, int nc, Span *nxt, int pass, int printing) {
    int i;
    int nn = 0;
    if (printing) {
        printf("pass %d\n", pass);
    }
    for (i = 0; i < nc; i++) {
        int at = quick_partition(s, cur[i].lo, cur[i].hi);
        if (printing) {
            printf("cut %d %d pivot=%d at=%d\n", cur[i].lo, cur[i].hi, s->a[at].key, at);
            seq_print(s);
        }
        nn = push_span(nxt, nn, cur[i].lo, at - 1);
        nn = push_span(nxt, nn, at + 1, cur[i].hi);
    }
    return nn;
}

static int quick_run(Seq *s, Seq *snap, int cap, int printing) {
    Span cur[SORT_MAX];
    Span nxt[SORT_MAX];
    int nc;
    int pass = 0;
    int i;
    if (s->n <= 1) {
        if (printing) {
            printf("sorted\n");
            seq_print(s);
            seq_stats(s);
        }
        return 0;
    }
    nc = push_span(cur, 0, 0, s->n - 1);
    while (nc > 0 && pass < cap) {
        pass++;
        nc = one_pass(s, cur, nc, nxt, pass, printing);
        if (snap != NULL) {
            snap[pass - 1] = *s;
        }
        for (i = 0; i < nc; i++) {
            cur[i] = nxt[i];
        }
    }
    if (printing) {
        seq_stats(s);
    }
    return pass;
}

void quick_trace(Seq *s) {
    quick_run(s, NULL, SORT_MAX, 1);
}

int quick_collect(const Seq *src, Seq *out, int cap) {
    Seq s = *src;
    return quick_run(&s, out, cap, 0);
}

static int range_ok(const Seq *s, int lo, int hi, int k) {
    int p;
    if (k <= 0 || hi <= lo) {
        return 1;
    }
    for (p = lo; p <= hi; p++) {
        if (!seq_final_at(s, p)) {
            continue;
        }
        if (range_ok(s, lo, p - 1, k - 1) && range_ok(s, p + 1, hi, k - 1)) {
            return 1;
        }
    }
    return 0;
}

int quick_possible(const Seq *s, int passes) {
    if (s->n <= 1) {
        return 1;
    }
    return range_ok(s, 0, s->n - 1, passes);
}

typedef struct {
    long parts;
    int depth;
    int max_depth;
} Walk;

static void dive(Seq *s, int lo, int hi, int order, Walk *w);

static void dive_pair(Seq *s, int lo, int hi, int at, int order, Walk *w) {
    int left = at - lo;
    int right = hi - at;
    int right_first = 0;
    if (order == Q_RIGHT) {
        right_first = 1;
    } else if (order == Q_SHORT && right < left) {
        right_first = 1;
    } else if (order == Q_LONG && right > left) {
        right_first = 1;
    }
    if (right_first) {
        dive(s, at + 1, hi, order, w);
        dive(s, lo, at - 1, order, w);
    } else {
        dive(s, lo, at - 1, order, w);
        dive(s, at + 1, hi, order, w);
    }
}

static void dive(Seq *s, int lo, int hi, int order, Walk *w) {
    int at;
    w->depth++;
    if (w->depth > w->max_depth) {
        w->max_depth = w->depth;
    }
    if (lo < hi) {
        w->parts++;
        at = quick_partition(s, lo, hi);
        dive_pair(s, lo, hi, at, order, w);
    }
    w->depth--;
}

static void walk_opt(Seq *s, int lo, int hi, Walk *w) {
    w->depth++;
    if (w->depth > w->max_depth) {
        w->max_depth = w->depth;
    }
    while (lo < hi) {
        int at = quick_partition(s, lo, hi);
        w->parts++;
        if (at - lo < hi - at) {
            walk_opt(s, lo, at - 1, w);
            lo = at + 1;
        } else {
            walk_opt(s, at + 1, hi, w);
            hi = at - 1;
        }
    }
    w->depth--;
}

void quick_recur(const Seq *src, int order, long *parts, int *depth) {
    Seq s = *src;
    Walk w = {0, 0, 0};
    dive(&s, 0, s.n - 1, order, &w);
    *parts = w.parts;
    *depth = w.max_depth;
}

void quick_opt(const Seq *src, long *parts, int *depth) {
    Seq s = *src;
    Walk w = {0, 0, 0};
    if (s.n > 0) {
        walk_opt(&s, 0, s.n - 1, &w);
    }
    *parts = w.parts;
    *depth = w.max_depth;
}
