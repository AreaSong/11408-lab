#include "frac.h"

#include <limits.h>
#include <stdio.h>

static long long igcd(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

Frac frac_make(long long n, long long d) {
    Frac f;
    long long g;
    if (d < 0) {
        n = -n;
        d = -d;
    }
    if (d == 0) {
        d = 1;
    }
    g = igcd(n, d);
    f.n = n / g;
    f.d = d / g;
    return f;
}

static int mul_pos(long long a, long long b, long long *out) {
    if (a < 0 || b < 0) {
        return -1;
    }
    if (a != 0 && b > LLONG_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

int frac_add(Frac *acc, Frac b) {
    long long n1, n2, d;
    if (acc->n < 0 || b.n < 0 || acc->d <= 0 || b.d <= 0) {
        return -1;
    }
    if (mul_pos(acc->n, b.d, &n1) || mul_pos(b.n, acc->d, &n2)) {
        return -1;
    }
    if (n1 > LLONG_MAX - n2) {
        return -1;
    }
    if (mul_pos(acc->d, b.d, &d)) {
        return -1;
    }
    *acc = frac_make(n1 + n2, d);
    return 0;
}

int frac_div_int(Frac a, long long k, Frac *out) {
    long long d;
    if (k <= 0 || a.n < 0 || a.d <= 0) {
        return -1;
    }
    if (mul_pos(a.d, k, &d)) {
        return -1;
    }
    *out = frac_make(a.n, d);
    return 0;
}

int frac_fmt(char *buf, size_t n, Frac f) {
    return snprintf(buf, n, "%lld/%lld", f.n, f.d);
}
