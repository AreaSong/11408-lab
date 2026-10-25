#include "bound.h"

int bound_equal(int n, int k, long long *least) {
    if (n < 1 || k < 1) {
        return -1;
    }
    *least = (long long)n * (k - 1) + 1;
    return 0;
}

int bound_list(const int *k, int n, long long *least, long long *worst) {
    int i;
    long long sum = 0;
    if (n < 1) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (k[i] < 1) {
            return -1;
        }
        sum += (long long)k[i] - 1;
    }
    *worst = sum;
    *least = sum + 1;
    return 0;
}

int bound_maxn(int m, int k, int *out) {
    if (m < 1 || k < 2) {
        return -1;
    }
    *out = (m - 1) / (k - 1);
    return 0;
}

int bound_mink(int m, int k, int *out) {
    int maxn;
    if (bound_maxn(m, k, &maxn) != 0) {
        return -1;
    }
    *out = maxn + 1;
    return 0;
}
