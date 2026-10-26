#include "bubble.h"

#include <stdio.h>

static int pass_sink(Seq *s, int limit) {
    int j;
    int swaps = 0;
    for (j = 0; j < limit; j++) {
        s->cmp++;
        if (s->a[j].key > s->a[j + 1].key) {
            seq_swap(s, j, j + 1);
            swaps++;
        }
    }
    return swaps;
}

static int pass_rise(Seq *s, int start) {
    int j;
    int swaps = 0;
    for (j = s->n - 1; j > start; j--) {
        s->cmp++;
        if (s->a[j - 1].key > s->a[j].key) {
            seq_swap(s, j - 1, j);
            swaps++;
        }
    }
    return swaps;
}

static int bubble_run(Seq *s, int sink, Seq *out, int cap, int printing) {
    int pass;
    int npass = 0;
    if (s->n <= 1) {
        if (printing) {
            printf("sorted\n");
            seq_print(s);
            seq_stats(s);
        }
        return 0;
    }
    for (pass = 1; pass < s->n; pass++) {
        int swaps = sink ? pass_sink(s, s->n - pass) : pass_rise(s, pass - 1);
        npass++;
        if (printing) {
            printf("pass %d swap=%d %s=%d\n", pass, swaps, sink ? "tail" : "head", pass);
            seq_print(s);
        }
        if (out != NULL && npass <= cap) {
            out[npass - 1] = *s;
        }
        if (swaps == 0) {
            if (printing) {
                printf("stop\n");
            }
            break;
        }
    }
    if (printing) {
        seq_stats(s);
    }
    return npass;
}

void bubble_trace(Seq *s, int sink) {
    bubble_run(s, sink, NULL, 0, 1);
}

int bubble_collect(const Seq *src, Seq *out, int cap, int sink) {
    Seq s = *src;
    return bubble_run(&s, sink, out, cap, 0);
}
