#ifndef NUM_H
#define NUM_H

#include <string.h>

/* 真题矩阵不超过 5×3。留到 8，计数时 8! 仍能在瞬间算完。 */
#define BANK_N 8
#define BANK_M 8
#define BANK_MAXV 1000000

typedef struct {
    int who;
    int work[BANK_M];
    int ask[BANK_M];
    int alloc[BANK_M];
    int next[BANK_M];
} TraceStep;

static inline int num_leq(const int *a, const int *b, int m) {
    int i;
    for (i = 0; i < m; i++) {
        if (a[i] > b[i]) {
            return 0;
        }
    }
    return 1;
}

static inline int num_any(const int *a, int m) {
    int i;
    for (i = 0; i < m; i++) {
        if (a[i] != 0) {
            return 1;
        }
    }
    return 0;
}

static inline void num_copy(int *dst, const int *src, int n) {
    memcpy(dst, src, (size_t)n * sizeof(int));
}

static inline void num_add(int *dst, const int *a, const int *b, int m) {
    int i;
    for (i = 0; i < m; i++) {
        dst[i] = a[i] + b[i];
    }
}

static inline void num_zero(int *a, int n) {
    memset(a, 0, (size_t)n * sizeof(int));
}

#endif
