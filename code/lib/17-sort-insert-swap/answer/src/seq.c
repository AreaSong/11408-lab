#include "seq.h"

#include <stdio.h>

void seq_clear(Seq *s) {
    s->n = 0;
    s->cmp = 0;
    s->move = 0;
}

void seq_load(Seq *s, const int *key, int n) {
    int i;
    seq_clear(s);
    s->n = n;
    for (i = 0; i < n; i++) {
        s->a[i].key = key[i];
        s->a[i].ord = i;
    }
}

int seq_same(const Seq *a, const Seq *b) {
    int i;
    if (a->n != b->n) {
        return 0;
    }
    for (i = 0; i < a->n; i++) {
        if (a->a[i].key != b->a[i].key) {
            return 0;
        }
    }
    return 1;
}

int seq_ordered(const Seq *s, int lo, int hi) {
    int i;
    for (i = lo; i < hi; i++) {
        if (s->a[i].key > s->a[i + 1].key) {
            return 0;
        }
    }
    return 1;
}

int seq_sorted(const Seq *s) {
    if (s->n <= 1) {
        return 1;
    }
    return seq_ordered(s, 0, s->n - 1);
}

/* 左面都不比它大、右面都不比它小，说明它已经在最终位置。相等算就位。 */
int seq_final_at(const Seq *s, int i) {
    int j;
    int v = s->a[i].key;
    for (j = 0; j < i; j++) {
        if (s->a[j].key > v) {
            return 0;
        }
    }
    for (j = i + 1; j < s->n; j++) {
        if (s->a[j].key < v) {
            return 0;
        }
    }
    return 1;
}

int seq_fixed(const Seq *s) {
    int i;
    int n = 0;
    for (i = 0; i < s->n; i++) {
        if (seq_final_at(s, i)) {
            n++;
        }
    }
    return n;
}

int seq_stable(const Seq *s) {
    int i;
    int j;
    for (i = 0; i < s->n; i++) {
        for (j = i + 1; j < s->n; j++) {
            if (s->a[i].key == s->a[j].key && s->a[i].ord > s->a[j].ord) {
                return 0;
            }
        }
    }
    return 1;
}

void seq_print(const Seq *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        if (i > 0) {
            putchar(' ');
        }
        printf("%d", s->a[i].key);
    }
    putchar('\n');
}

void seq_print_ord(const Seq *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        if (i > 0) {
            putchar(' ');
        }
        printf("%d:%d", s->a[i].key, s->a[i].ord);
    }
    putchar('\n');
}

void seq_stats(const Seq *s) {
    printf("cmp=%ld\nmove=%ld\n", s->cmp, s->move);
}

/* 一次交换是三次记录赋值。关键字相等时调用方不交换，稳定性靠这一点。 */
void seq_swap(Seq *s, int i, int j) {
    Rec t;
    if (i == j) {
        return;
    }
    t = s->a[i];
    s->move++;
    s->a[i] = s->a[j];
    s->move++;
    s->a[j] = t;
    s->move++;
}
