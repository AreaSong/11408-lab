#include "qty.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int same(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* 非负十进制乘上整数倍率，只接受离某个整数极近的结果 */
static int scale_ll(double value, long long scale, long long *out) {
    long double prod;
    long long rounded;
    long double err;

    if (!(value >= 0) || value > 1e15 || scale <= 0) {
        return 0;
    }
    prod = (long double)value * (long double)scale;
    if (prod > (long double)LLONG_MAX) {
        return 0;
    }
    rounded = (long long)(prod + 0.5L);
    err = prod - (long double)rounded;
    if (err < 0) {
        err = -err;
    }
    if (err > 0.001L) {
        return 0;
    }
    *out = rounded;
    return 1;
}

bool parse_bits(double value, const char *unit, long long *bits) {
    static const struct {
        const char *name;
        long long scale;
    } table[] = {
        {"bit", 1}, {"b", 1},
        {"kbit", 1000}, {"kb", 1000},
        {"Mbit", 1000000LL}, {"Mb", 1000000LL},
        {"Gbit", 1000000000LL}, {"Gb", 1000000000LL},
        {"B", 8}, {"kB", 8000}, {"KB", 8000},
        {"MB", 8000000LL}, {"GB", 8000000000LL},
    };
    size_t i;

    if (unit == NULL || bits == NULL) {
        return false;
    }
    for (i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (same(unit, table[i].name)) {
            return scale_ll(value, table[i].scale, bits);
        }
    }
    return false;
}

bool parse_bps(double value, const char *unit, long long *bps) {
    static const struct {
        const char *name;
        long long scale;
    } table[] = {
        {"bps", 1}, {"b/s", 1}, {"bit/s", 1},
        {"kbps", 1000}, {"kb/s", 1000}, {"kbit/s", 1000},
        {"Mbps", 1000000LL}, {"Mb/s", 1000000LL}, {"Mbit/s", 1000000LL},
        {"Gbps", 1000000000LL}, {"Gb/s", 1000000000LL}, {"Gbit/s", 1000000000LL},
    };
    size_t i;
    long long raw;

    if (unit == NULL || bps == NULL) {
        return false;
    }
    for (i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (!same(unit, table[i].name)) {
            continue;
        }
        if (!scale_ll(value, table[i].scale, &raw) || raw <= 0) {
            return false;
        }
        *bps = raw;
        return true;
    }
    return false;
}

bool parse_ns(double value, const char *unit, long long *ns) {
    static const struct {
        const char *name;
        long long scale;
    } table[] = {
        {"ns", 1}, {"us", 1000}, {"ms", 1000000LL}, {"s", 1000000000LL},
    };
    size_t i;

    if (unit == NULL || ns == NULL) {
        return false;
    }
    for (i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (same(unit, table[i].name)) {
            return scale_ll(value, table[i].scale, ns);
        }
    }
    return false;
}

bool parse_meters(double value, const char *unit, long long *meters) {
    if (unit == NULL || meters == NULL) {
        return false;
    }
    if (same(unit, "m")) {
        return scale_ll(value, 1, meters);
    }
    if (same(unit, "km")) {
        return scale_ll(value, 1000, meters);
    }
    return false;
}

bool parse_mps(double value, const char *unit, long long *mps) {
    long long raw;

    if (unit == NULL || mps == NULL) {
        return false;
    }
    if (!same(unit, "m/s") && !same(unit, "mps")) {
        return false;
    }
    if (!scale_ll(value, 1, &raw) || raw <= 0) {
        return false;
    }
    *mps = raw;
    return true;
}

bool qty_mul_div(long long a, long long b, long long den, long long *out) {
    if (out == NULL || den <= 0 || a < 0 || b < 0) {
        return false;
    }
    if (a > 0 && b > LLONG_MAX / a) {
        return false;
    }
    if ((a * b) % den != 0) {
        return false;
    }
    *out = (a * b) / den;
    return true;
}

bool qty_div_ns(long long amount, long long per_sec, long long *ns) {
    return qty_mul_div(amount, 1000000000LL, per_sec, ns);
}

/* min_frac：有小数时至少保留几位。80.10 ms 不能收成 80.1，否则和选项对不上 */
static void fmt_dec(char *buf, size_t n, long long value, long long scale, int min_frac) {
    long long ip = value / scale;
    long long fp = value % scale;
    char frac[24];
    char *end;
    int width = 0;
    int len;
    long long probe = scale;

    if (fp == 0) {
        snprintf(buf, n, "%lld", ip);
        return;
    }
    while (probe > 1) {
        width++;
        probe /= 10;
    }
    snprintf(frac, sizeof frac, "%0*lld", width, fp);
    end = frac + strlen(frac) - 1;
    while (end > frac && *end == '0') {
        *end = '\0';
        end--;
    }
    len = (int)strlen(frac);
    while (len < min_frac && len < (int)sizeof frac - 1) {
        frac[len++] = '0';
        frac[len] = '\0';
    }
    snprintf(buf, n, "%lld.%s", ip, frac);
}

static void append_bit_scale(char *buf, size_t n, long long bits) {
    size_t used = strlen(buf);

    if (bits % 1000000000LL == 0) {
        snprintf(buf + used, n - used, " = %lld Gbit（G = 10^9）", bits / 1000000000LL);
    } else if (bits % 1000000LL == 0) {
        snprintf(buf + used, n - used, " = %lld Mbit（M = 10^6）", bits / 1000000LL);
    } else if (bits % 1000 == 0) {
        snprintf(buf + used, n - used, " = %lld kbit（k = 10^3）", bits / 1000);
    }
}

static void append_bytes(char *buf, size_t n, long long bits) {
    long long bytes;
    size_t used;

    if (bits % 8 != 0) {
        return;
    }
    bytes = bits / 8;
    used = strlen(buf);
    snprintf(buf + used, n - used, " = %lld B", bytes);
    used = strlen(buf);
    if (bytes % 1000000LL == 0 && bytes != 0) {
        snprintf(buf + used, n - used, " = %lld MB（M = 10^6）", bytes / 1000000LL);
    } else if (bytes % 1000 == 0 && bytes >= 1000) {
        snprintf(buf + used, n - used, " = %lld kB（k = 10^3）", bytes / 1000);
    }
}

void fmt_bits(char *buf, size_t n, long long bits) {
    if (bits == 0) {
        snprintf(buf, n, "0 bit");
        return;
    }
    snprintf(buf, n, "%lld bit", bits);
    /* 10 kb 这种写法要能在输出里看见 kbit，同时保留字节数 */
    append_bit_scale(buf, n, bits);
    append_bytes(buf, n, bits);
}

void fmt_bps(char *buf, size_t n, long long bps) {
    if (bps % 1000000000LL == 0 && bps != 0) {
        snprintf(buf, n, "%lld bps = %lld Gbps（G = 10^9）", bps, bps / 1000000000LL);
    } else if (bps % 1000000LL == 0 && bps != 0) {
        snprintf(buf, n, "%lld bps = %lld Mbps（M = 10^6）", bps, bps / 1000000LL);
    } else if (bps % 1000 == 0 && bps != 0) {
        snprintf(buf, n, "%lld bps = %lld kbps（k = 10^3）", bps, bps / 1000);
    } else {
        snprintf(buf, n, "%lld bps", bps);
    }
}

void fmt_meters(char *buf, size_t n, long long meters) {
    if (meters % 1000 == 0 && meters != 0) {
        snprintf(buf, n, "%lld m = %lld km", meters, meters / 1000);
    } else {
        snprintf(buf, n, "%lld m", meters);
    }
}

void fmt_time(char *buf, size_t n, long long ns) {
    char sec[64];
    char ms[64];

    if (ns <= 0) {
        snprintf(buf, n, "0 s");
        return;
    }
    fmt_dec(sec, sizeof sec, ns, 1000000000LL, 0);
    fmt_dec(ms, sizeof ms, ns, 1000000LL, 2);
    snprintf(buf, n, "%s s = %s ms", sec, ms);
}

void fmt_time_one(char *buf, size_t n, long long ns) {
    char ms[64];

    if (ns % 1000000000LL == 0) {
        snprintf(buf, n, "%lld s", ns / 1000000000LL);
        return;
    }
    fmt_dec(ms, sizeof ms, ns, 1000000LL, 2);
    snprintf(buf, n, "%s ms", ms);
}

void fmt_time_ms(char *buf, size_t n, long long ns) {
    char ms[64];

    fmt_dec(ms, sizeof ms, ns, 1000000LL, 2);
    snprintf(buf, n, "%s ms", ms);
}

void fmt_span(char *buf, size_t n, long long bits) {
    if (bits == 0) {
        snprintf(buf, n, "0 bit");
        return;
    }
    snprintf(buf, n, "%lld bit", bits);
    append_bit_scale(buf, n, bits);
}

void qty_print_scale_note(FILE *out) {
    fputs("单位约定：k、M、G 按 10^3、10^6、10^9 计算；1 B = 8 bit。\n", out);
}
