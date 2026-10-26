#include "ratio.h"

#include <stdio.h>

static long long gcd_ll(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    while (b != 0) {
        long long r = a % b;
        a = b;
        b = r;
    }
    return a == 0 ? 1 : a;
}

int ratio_make(long long num, long long den, Ratio *out) {
    long long g;

    if (out == NULL || den <= 0 || num < 0) {
        return -1;
    }
    g = gcd_ll(num, den);
    out->num = num / g;
    out->den = den / g;
    return 0;
}

/* 交叉相乘会到 10^19 以上，long long 装不下。 */
int ratio_cmp(Ratio a, Ratio b) {
    __int128 left = (__int128)a.num * (__int128)b.den;
    __int128 right = (__int128)b.num * (__int128)a.den;

    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}

void ratio_fmt(char *buf, size_t n, Ratio t) {
    __int128 scaled;
    long long milli;

    if (t.den == 1) {
        snprintf(buf, n, "%lld s", t.num);
        return;
    }
    scaled = (__int128)t.num * 1000 + t.den / 2;
    milli = (long long)(scaled / t.den);
    snprintf(buf, n, "%lld/%lld s（约 %lld.%03lld s）", t.num, t.den, milli / 1000, milli % 1000);
}
