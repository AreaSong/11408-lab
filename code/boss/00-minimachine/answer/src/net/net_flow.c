#include "net/net_flow.h"

#include <stdio.h>
#include <string.h>

int net_flow_left(int cwnd, int rwnd, int inflight) {
    int sw = cwnd < rwnd ? cwnd : rwnd;
    return sw - inflight;
}

int net_flow_zero(int read_first, unsigned *seq, int *rwnd) {
    if (seq == NULL || rwnd == NULL) {
        return -1;
    }
    /* 三段 100 字节把 300 字节缓存填满，下一序号是 301。 */
    *seq = 301u;
    if (!read_first) {
        *rwnd = 0;
        return 0;
    }
    /* 应用读空了，通告丢失。探测的 1 字节被收下，窗口剩 299。 */
    *rwnd = 300 - 1;
    return 0;
}

static long smooth(long old, long sample, int numer, int denom) {
    return ((denom - numer) * old + numer * sample) / denom;
}

static long abs_diff(long a, long b) { return a >= b ? a - b : b - a; }

int net_rto_pair(long *rto_us, long *karn_us) {
    long rtts;
    long rttd;
    long gap;
    if (rto_us == NULL || karn_us == NULL) {
        return -1;
    }
    /* 第一个样本把初始的 6 s 换掉，不和 6 s 做加权。 */
    rtts = 1500000L;
    rttd = rtts / 2;
    *rto_us = rtts + 4 * rttd;
    rtts = smooth(rtts, 2500000L, 1, 8);
    gap = abs_diff(rtts, 2500000L);
    rttd = smooth(rttd, gap, 1, 4);
    *rto_us = rtts + 4 * rttd;
    /* Karn：重传把 RTO 翻倍，紧接着的样本不更新估计。 */
    *karn_us = *rto_us * 2;
    return 0;
}

static int fmt_sec(char *out, int cap, long us) {
    long sec = us / 1000000L;
    long frac = us % 1000000L;
    char digits[8];
    int i;
    if (frac == 0) {
        return snprintf(out, (size_t)cap, "%ld", sec);
    }
    snprintf(digits, sizeof digits, "%06ld", frac);
    i = 5;
    while (i > 0 && digits[i] == '0') {
        i--;
    }
    digits[i + 1] = '\0';
    return snprintf(out, (size_t)cap, "%ld.%s", sec, digits);
}

int net_rto_line(char *out, int cap) {
    long second = 0;
    long karn = 0;
    long rtts = 1500000L;
    long rttd = 750000L;
    long first = rtts + 4 * rttd;
    char a[16];
    char b[16];
    char c[16];
    char d[16];
    char e[16];
    char f[16];
    char g[16];
    int n;
    if (net_rto_pair(&second, &karn) != 0) {
        return -1;
    }
    rtts = smooth(1500000L, 2500000L, 1, 8);
    rttd = smooth(750000L, abs_diff(rtts, 2500000L), 1, 4);
    if (fmt_sec(a, (int)sizeof a, 1500000L) < 0 || fmt_sec(b, (int)sizeof b, 750000L) < 0 ||
        fmt_sec(c, (int)sizeof c, first) < 0 || fmt_sec(d, (int)sizeof d, rtts) < 0 ||
        fmt_sec(e, (int)sizeof e, rttd) < 0 || fmt_sec(f, (int)sizeof f, second) < 0 ||
        fmt_sec(g, (int)sizeof g, karn) < 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "rtts=%s dev=%s rto=%s rtts2=%s dev2=%s rto2=%s karn=%s", a, b, c, d, e,
                 f, g);
    return n > 0 && n < cap ? 0 : -1;
}

int net_flow_line(const char *id, char *out, int cap) {
    int n;
    unsigned seq = 0;
    int rwnd = 0;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2010-39") == 0) {
        n = snprintf(out, (size_t)cap, "left=%d letter=A", net_flow_left(4000, 2000, 1000));
    } else if (strcmp(id, "zero") == 0) {
        if (net_flow_zero(1, &seq, &rwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "seq=%u rwnd=%d", seq, rwnd);
    } else if (strcmp(id, "probe") == 0) {
        if (net_flow_zero(0, &seq, &rwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "seq=%u rwnd=%d", seq, rwnd);
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
