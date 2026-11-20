#include "shell.h"

#include "insert.h"

#include <stdio.h>

int shell_halve(int n, int *gaps) {
    int d;
    int k = 0;
    for (d = n / 2; d >= 1; d /= 2) {
        gaps[k++] = d;
    }
    return k;
}

void shell_pass(Seq *s, int d) {
    int i;
    for (i = d; i < s->n; i++) {
        gap_insert(s, i, d);
    }
}

void shell_trace(Seq *s, const int *gaps, int ng) {
    int p;
    if (s->n <= 1) {
        printf("sorted\n");
        seq_print(s);
        seq_stats(s);
        return;
    }
    printf("gaps");
    for (p = 0; p < ng; p++) {
        printf(" %d", gaps[p]);
    }
    putchar('\n');
    for (p = 0; p < ng; p++) {
        shell_pass(s, gaps[p]);
        printf("pass %d gap=%d\n", p + 1, gaps[p]);
        seq_print(s);
    }
    if (ng == 0 || gaps[ng - 1] != 1) {
        printf("partial=yes\n");
    }
    seq_stats(s);
}

static int search_gaps(const Seq *init, const Seq *pass, int np, int k, int *got) {
    int g;
    if (k == np) {
        Seq s = *init;
        int i;
        for (i = 0; i < np; i++) {
            shell_pass(&s, got[i]);
            if (!seq_same(&s, &pass[i])) {
                return 0;
            }
        }
        return 1;
    }
    for (g = 1; g < init->n; g++) {
        got[k] = g;
        if (search_gaps(init, pass, np, k + 1, got)) {
            return 1;
        }
    }
    return 0;
}

int shell_match(const Seq *init, const Seq *pass, int np, int *got) {
    int tmp[8];
    int *slot = got != NULL ? got : tmp;
    if (np <= 0 || np > 4 || init->n <= 1) {
        return 0;
    }
    return search_gaps(init, pass, np, 0, slot);
}
