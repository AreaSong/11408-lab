#include "units.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int parse_whole(const char *s, long long *whole, const char **end) {
    long long v = 0;
    int any = 0;

    while (*s >= '0' && *s <= '9') {
        any = 1;
        if (v > (LLONG_MAX - (*s - '0')) / 10) {
            return -1;
        }
        v = v * 10 + (*s - '0');
        s++;
    }
    if (!any) {
        return -1;
    }
    *whole = v;
    *end = s;
    return 0;
}

static int suffix_eq(const char *s, const char *word) {
    size_t n = strlen(word);
    size_t i;

    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)s[i]) != word[i]) {
            return 0;
        }
    }
    return s[n] == '\0';
}

int parse_bytes(const char *s, long *out) {
    long long whole = 0;
    long long scale = 1;
    const char *end = s;

    if (parse_whole(s, &whole, &end) != 0 || whole <= 0) {
        return -1;
    }
    while (*end == ' ') {
        end++;
    }
    if (*end == '\0') {
        scale = 1;
    } else if (suffix_eq(end, "kb") || suffix_eq(end, "k")) {
        scale = 1024;
    } else if (suffix_eq(end, "b")) {
        scale = 1;
    } else {
        return -1;
    }
    if (whole > 1073741824LL / scale) {
        return -1;
    }
    *out = (long)(whole * scale);
    return 0;
}

/* 小数先按位数收好。单位要等看到 s / ms 才知道，不能先用浮点。 */
static int scan_time_body(const char **ps, long long *whole, long long *frac, int *digits) {
    const char *p = *ps;
    int any = 0;

    *whole = 0;
    *frac = 0;
    *digits = 0;
    while (*p >= '0' && *p <= '9') {
        any = 1;
        if (*whole > (LLONG_MAX - (*p - '0')) / 10) {
            return -1;
        }
        *whole = *whole * 10 + (*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            any = 1;
            if (*digits >= 6) {
                return -1;
            }
            *frac = *frac * 10 + (*p - '0');
            *digits += 1;
            p++;
        }
    }
    if (!any) {
        return -1;
    }
    *ps = p;
    return 0;
}

int parse_time_us(const char *s, long *out) {
    const char *p = s;
    long long whole, frac, unit, place, total;
    int digits, i;

    if (scan_time_body(&p, &whole, &frac, &digits) != 0) {
        return -1;
    }
    while (*p == ' ') {
        p++;
    }
    if (suffix_eq(p, "ms")) {
        unit = 1000;
    } else if (suffix_eq(p, "s")) {
        unit = 1000000;
    } else {
        return -1;
    }
    place = unit;
    for (i = 0; i < digits; i++) {
        if (place % 10 != 0) {
            return -1;
        }
        place /= 10;
    }
    total = whole * unit + frac * place;
    if (total <= 0 || total > 86400LL * 1000000LL) {
        return -1;
    }
    *out = (long)total;
    return 0;
}

int parse_count(const char *s, int *out) {
    long long v = 0;
    const char *end = s;

    if (parse_whole(s, &v, &end) != 0 || *end != '\0' || v <= 0 || v > 100000) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

void fmt_bytes(long n, char *out, size_t cap) {
    if (n >= 1024 && n % 1024 == 0) {
        snprintf(out, cap, "%ld KB", n / 1024);
    } else {
        snprintf(out, cap, "%ld 字节", n);
    }
}

void fmt_seconds(long us, char *out, size_t cap) {
    long sec = us / 1000000;
    long frac = us % 1000000;
    int digits = 6;

    if (frac == 0) {
        snprintf(out, cap, "%ld s", sec);
        return;
    }
    while (digits > 1 && frac % 10 == 0) {
        frac /= 10;
        digits--;
    }
    snprintf(out, cap, "%ld.%0*ld s", sec, digits, frac);
}

void fmt_time_us(long us, char *out, size_t cap) {
    if (us >= 0 && us < 1000000 && us % 1000 == 0) {
        snprintf(out, cap, "%ld ms", us / 1000);
        return;
    }
    fmt_seconds(us, out, cap);
}
