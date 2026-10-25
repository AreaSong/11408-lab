#include "unit.h"

#include <limits.h>
#include <string.h>

static int same(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* 只接受离某个整数极近的结果，避免 0.1 的二进制尾巴混进速率 */
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

static int parse_scaled(double value, const char *unit, long long *out,
                        const char *const *names, const long long *scales, int count,
                        int positive) {
    int i;
    long long raw;

    if (unit == NULL || out == NULL) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (!same(unit, names[i])) {
            continue;
        }
        if (!scale_ll(value, scales[i], &raw)) {
            return 0;
        }
        if (positive && raw <= 0) {
            return 0;
        }
        *out = raw;
        return 1;
    }
    return 0;
}

bool parse_hz(double value, const char *unit, long long *hz) {
    static const char *names[] = {"Hz", "kHz", "MHz", "GHz"};
    static const long long scales[] = {1, 1000, 1000000LL, 1000000000LL};
    return parse_scaled(value, unit, hz, names, scales, 4, 1);
}

bool parse_bps(double value, const char *unit, long long *bps) {
    static const char *names[] = {
        "bps", "b/s", "bit/s",
        "kbps", "kb/s", "kbit/s",
        "Mbps", "Mb/s", "Mbit/s",
        "Gbps", "Gb/s", "Gbit/s",
    };
    static const long long scales[] = {
        1, 1, 1,
        1000, 1000, 1000,
        1000000LL, 1000000LL, 1000000LL,
        1000000000LL, 1000000000LL, 1000000000LL,
    };
    return parse_scaled(value, unit, bps, names, scales, 12, 1);
}

bool parse_baud(double value, const char *unit, long long *baud) {
    static const char *names[] = {"Baud", "kBaud", "MBaud", "GBaud"};
    static const long long scales[] = {1, 1000, 1000000LL, 1000000000LL};
    return parse_scaled(value, unit, baud, names, scales, 4, 1);
}

bool parse_count(double value, long long *out) {
    if (out == NULL) {
        return false;
    }
    return scale_ll(value, 1, out);
}

static void fmt_scaled(char *buf, size_t n, long long value, const char *base,
                       const char *kilo, const char *mega, const char *giga) {
    if (value % 1000000000LL == 0 && value != 0) {
        snprintf(buf, n, "%lld %s = %lld %s（G = 10^9）", value, base,
                 value / 1000000000LL, giga);
    } else if (value % 1000000LL == 0 && value != 0) {
        snprintf(buf, n, "%lld %s = %lld %s（M = 10^6）", value, base,
                 value / 1000000LL, mega);
    } else if (value % 1000 == 0 && value != 0) {
        snprintf(buf, n, "%lld %s = %lld %s（k = 10^3）", value, base, value / 1000, kilo);
    } else {
        snprintf(buf, n, "%lld %s", value, base);
    }
}

void fmt_hz(char *buf, size_t n, long long hz) {
    fmt_scaled(buf, n, hz, "Hz", "kHz", "MHz", "GHz");
}

void fmt_bps(char *buf, size_t n, long long bps) {
    fmt_scaled(buf, n, bps, "bps", "kbps", "Mbps", "Gbps");
}

void fmt_baud(char *buf, size_t n, long long baud) {
    fmt_scaled(buf, n, baud, "Baud", "kBaud", "MBaud", "GBaud");
}

void unit_print_scale_note(FILE *out) {
    fputs("单位约定：k、M、G 按 10^3、10^6、10^9 计算。\n", out);
}

int floor_log2_u(unsigned long long x) {
    int k = 0;

    if (x == 0) {
        return -1;
    }
    while (x > 1) {
        x >>= 1;
        k++;
    }
    return k;
}

bool mul_ll(long long a, long long b, long long *out) {
    if (out == NULL || a < 0 || b < 0) {
        return false;
    }
    if (a > 0 && b > LLONG_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}
