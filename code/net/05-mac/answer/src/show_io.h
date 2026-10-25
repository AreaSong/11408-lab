#ifndef SHOW_IO_H
#define SHOW_IO_H

#include "limit.h"
#include "qty.h"
#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static inline int need_key(const char **ps, const char *key) {
    char word[32];

    return scan_word(ps, word, (int)sizeof word) == 1 && strcmp(word, key) == 0;
}

static inline int take_num(const char **ps, bool (*parse)(double, const char *, long long *), long long *out) {
    char num[64];
    char unit[32];
    char *end = NULL;
    double value;

    if (scan_word(ps, num, (int)sizeof num) != 1 || scan_word(ps, unit, (int)sizeof unit) != 1) {
        return 0;
    }
    value = strtod(num, &end);
    if (end == num || *end != '\0') {
        return 0;
    }
    return parse(value, unit, out);
}

static inline int optional_dev(const char **ps, long long *dev) {
    char word[32];
    int rc = scan_word(ps, word, (int)sizeof word);

    *dev = 0;
    if (rc == 0) {
        return 1;
    }
    if (rc != 1 || strcmp(word, "dev") != 0 || !take_num(ps, parse_ns, dev)) {
        return 0;
    }
    return scan_word(ps, word, (int)sizeof word) == 0;
}

static inline int take_int(const char **ps, int *out, int lo, int hi) {
    char word[32];
    char *end = NULL;
    long value;

    if (scan_word(ps, word, (int)sizeof word) != 1) {
        return 0;
    }
    value = strtol(word, &end, 10);
    if (end == word || *end != '\0' || value < lo || value > hi) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static inline void fmt_us(char *buf, size_t n, long long ns) {
    long long ip = ns / 1000;
    long long fp = ns % 1000;
    char frac[8];
    int len;

    if (ns < 0) {
        snprintf(buf, n, "?");
        return;
    }
    if (fp == 0) {
        snprintf(buf, n, "%lld us", ip);
        return;
    }
    snprintf(frac, sizeof frac, "%03lld", fp);
    len = 3;
    while (len > 0 && frac[len - 1] == '0') {
        frac[--len] = '\0';
    }
    snprintf(buf, n, "%lld.%s us", ip, frac);
}

static inline void fmt_rate(char *buf, size_t n, long long bps) {
    if (bps % 1000000000LL == 0 && bps != 0) {
        snprintf(buf, n, "%lld Gbps（G = 10^9）", bps / 1000000000LL);
    } else if (bps % 1000000LL == 0 && bps != 0) {
        snprintf(buf, n, "%lld Mbps（M = 10^6）", bps / 1000000LL);
    } else if (bps % 1000 == 0 && bps != 0) {
        snprintf(buf, n, "%lld kbps（k = 10^3）", bps / 1000);
    } else {
        snprintf(buf, n, "%lld bps", bps);
    }
}

static inline void fmt_speed(char *buf, size_t n, long long mps) {
    size_t used;

    snprintf(buf, n, "%lld m/s", mps);
    if (mps % 1000 == 0) {
        used = strlen(buf);
        snprintf(buf + used, n - used, " = %lld km/s", mps / 1000);
    }
    if (mps % 1000000LL == 0) {
        used = strlen(buf);
        snprintf(buf + used, n - used, " = %lld m/us", mps / 1000000LL);
    }
}

static inline int bad_span(void) {
    return err("这些量除不尽，或设备时延已经超过单程预算");
}

#endif
