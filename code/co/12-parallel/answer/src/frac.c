#include "frac.h"

static long long iabs(long long v) {
    return v < 0 ? -v : v;
}

static long long gcd_pos(long long a, long long b) {
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

void frac_reduce(long long num, long long den, long long *out_num, long long *out_den) {
    long long g;
    if (den < 0) {
        num = -num;
        den = -den;
    }
    g = gcd_pos(iabs(num), den);
    *out_num = num / g;
    *out_den = den / g;
}

void frac_write(FILE *fp, long long num, long long den) {
    if (den == 1) {
        fprintf(fp, "%lld", num);
        return;
    }
    fprintf(fp, "%lld/%lld", num, den);
}
