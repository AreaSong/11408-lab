#include "net/net_fmt.h"

#include <stdio.h>

static void put_frac(char *dst, int cap, long long whole, long long frac, const char *unit) {
    if (frac % 100 == 0) {
        snprintf(dst, (size_t)cap, "%lld %s", whole, unit);
        return;
    }
    snprintf(dst, (size_t)cap, "%lld.%02lld %s", whole, frac % 100, unit);
}

void net_fmt_ns(char *dst, int cap, long long ns) {
    long long x;
    if (dst == NULL || cap < 8) {
        return;
    }
    if (ns % 1000000000LL == 0) {
        snprintf(dst, (size_t)cap, "%lld s", ns / 1000000000LL);
        return;
    }
    /* 0.01 ms 为一格。不到 0.10 ms 的改用微秒，避免把 10 us 印成 0.01 ms。 */
    if (ns % 10000LL == 0) {
        x = ns / 10000LL;
        if (x >= 10) {
            put_frac(dst, cap, x / 100, x, "ms");
            return;
        }
    }
    if (ns % 1000LL == 0) {
        snprintf(dst, (size_t)cap, "%lld us", ns / 1000LL);
        return;
    }
    if (ns % 10LL == 0) {
        x = ns / 10LL;
        put_frac(dst, cap, x / 100, x, "us");
        return;
    }
    snprintf(dst, (size_t)cap, "%lld ns", ns);
}

void net_fmt_bps(char *dst, int cap, long long bps) {
    long long nearest;
    long long dist;
    long long centi;
    if (dst == NULL || cap < 8) {
        return;
    }
    if (bps >= 1000000LL && bps % 1000000LL == 0) {
        snprintf(dst, (size_t)cap, "%lld Mbps", bps / 1000000LL);
        return;
    }
    if (bps >= 1000LL && bps < 1000000LL && bps % 1000LL == 0) {
        snprintf(dst, (size_t)cap, "%lld kbps", bps / 1000LL);
        return;
    }
    if (bps < 1000000LL) {
        snprintf(dst, (size_t)cap, "%lld bps", bps);
        return;
    }
    /* 离整数 Mbps 不到 0.1 就收成那个整数，2014-36 的 79.936 记成约 80。 */
    nearest = (bps + 500000LL) / 1000000LL;
    dist = bps - nearest * 1000000LL;
    if (dist < 0) {
        dist = -dist;
    }
    if (dist < 100000LL) {
        snprintf(dst, (size_t)cap, "约 %lld Mbps", nearest);
        return;
    }
    centi = (bps * 100LL + 500000LL) / 1000000LL;
    snprintf(dst, (size_t)cap, "约 %lld.%02lld Mbps", centi / 100LL, centi % 100LL);
}

void net_fmt_pct(char *dst, int cap, long long num, long long den) {
    long long x;
    if (dst == NULL || cap < 8 || den <= 0) {
        return;
    }
    x = (num * 10000LL + den / 2) / den;
    snprintf(dst, (size_t)cap, "%lld.%02lld%%", x / 100LL, x % 100LL);
}
