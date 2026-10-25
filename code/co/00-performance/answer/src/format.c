#include "format.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool eq_ci(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (tolower(ca) != tolower(cb)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

double snap(double x) {
    double nearest;
    double ax;
    char tmp[64];

    if (!isfinite(x)) {
        return x;
    }
    if (x == 0) {
        return 0;
    }
    /*
     * 只把「差一点就是整数」的数收成整数，例如 1.2 GHz 换成 Hz 时的二进制误差。
     * 不能用相对 1 的容差：1 ns = 10^-9 s，会被收成 0。
     */
    nearest = round(x);
    if (fabs(nearest) >= 1.0 && fabs(x - nearest) < 1e-4 && fabs(nearest) < 1e15) {
        return nearest;
    }
    ax = fabs(x);
    /* 10^7 到 10^15 仍留小数：f/CPI 可能是 357142857.142857 这种，不能只留 6 位有效数字 */
    if (ax < 1e-4 || ax >= 1e15) {
        snprintf(tmp, sizeof tmp, "%.6g", x);
        return strtod(tmp, NULL);
    }
    return round(x * 1e6) / 1e6;
}

static void strip_trailing_zeros(char *buf) {
    char *dot = strchr(buf, '.');
    char *end;
    if (dot == NULL) {
        return;
    }
    end = buf + strlen(buf) - 1;
    while (end > dot && *end == '0') {
        *end = '\0';
        end--;
    }
    if (end == dot) {
        *end = '\0';
    }
}

void fmt_num(char *buf, size_t n, double x) {
    double y = snap(x);
    double ay = fabs(y);
    if (!isfinite(y)) {
        snprintf(buf, n, "nan");
        return;
    }
    /* 小于 1 的数不要因为离 0 很近就印成 0，时钟周期就是 10^-9 这一量级 */
    if (y == 0 || (ay >= 1.0 && fabs(y - round(y)) < 1e-6 && ay < 1e15)) {
        snprintf(buf, n, "%.0f", y);
        return;
    }
    if (ay >= 1e15 || ay < 1e-4) {
        snprintf(buf, n, "%.6g", y);
        return;
    }
    snprintf(buf, n, "%.6f", y);
    strip_trailing_zeros(buf);
}

bool parse_hz(double value, const char *unit, double *hz) {
    static const struct {
        const char *name;
        double scale;
    } units[] = {
        {"hz", 1}, {"khz", 1e3}, {"mhz", 1e6}, {"ghz", 1e9}, {"thz", 1e12},
    };
    double raw;
    size_t i;
    if (!(value > 0) || unit == NULL) {
        return false;
    }
    for (i = 0; i < sizeof units / sizeof units[0]; i++) {
        if (!eq_ci(unit, units[i].name)) {
            continue;
        }
        raw = value * units[i].scale;
        /* 容差只盖住二进制误差，不把 1.234 GHz 收成整数 Hz */
        if (fabs(raw - round(raw)) < 0.01) {
            raw = round(raw);
        }
        *hz = raw;
        return true;
    }
    return false;
}

void fmt_hz(char *buf, size_t n, double hz) {
    char h[64], m[64], g[64];
    fmt_num(h, sizeof h, hz);
    fmt_num(m, sizeof m, hz / 1e6);
    fmt_num(g, sizeof g, hz / 1e9);
    snprintf(buf, n, "%s Hz = %s MHz = %s GHz", h, m, g);
}

bool parse_seconds(double value, const char *unit, double *seconds) {
    static const struct {
        const char *name;
        double scale;
    } units[] = {
        {"s", 1}, {"ms", 1e-3}, {"us", 1e-6}, {"ns", 1e-9}, {"ps", 1e-12},
    };
    size_t i;
    if (!(value >= 0) || unit == NULL) {
        return false;
    }
    for (i = 0; i < sizeof units / sizeof units[0]; i++) {
        if (eq_ci(unit, units[i].name)) {
            *seconds = value * units[i].scale;
            return true;
        }
    }
    return false;
}

void fmt_seconds(char *buf, size_t n, double seconds) {
    static const struct {
        const char *name;
        double scale;
    } units[] = {
        {"ms", 1e3}, {"us", 1e6}, {"ns", 1e9}, {"ps", 1e12},
    };
    char base[64];
    size_t used;
    int i;

    fmt_num(base, sizeof base, seconds);
    used = (size_t)snprintf(buf, n, "%s s", base);
    /* 0.1 s 及以上不再换单位，否则 0.5 s 会变成 500 ms；更小的才展开成 ms/us/ns */
    if (!(seconds > 0) || seconds >= 0.1 || used >= n) {
        return;
    }
    for (i = 0; i < 4; i++) {
        double v = fabs(seconds * units[i].scale);
        char num[64];
        int wrote;
        if (v < 0.001 || v >= 1000 || used >= n) {
            continue;
        }
        fmt_num(num, sizeof num, seconds * units[i].scale);
        wrote = snprintf(buf + used, n - used, " = %s %s", num, units[i].name);
        if (wrote < 0) {
            return;
        }
        used += (size_t)wrote;
    }
}

bool parse_speedup(const char *text, double *speedup, bool *is_percent, double *percent) {
    char *end = NULL;
    if (text == NULL || text[0] == '\0') {
        return false;
    }
    if (text[0] == '+') {
        double p = strtod(text + 1, &end);
        if (end == text + 1 || strcmp(end, "%") != 0) {
            return false;
        }
        *is_percent = true;
        *percent = p;
        *speedup = 1.0 + p / 100.0;
        return *speedup > 0;
    }
    *speedup = strtod(text, &end);
    *is_percent = false;
    *percent = 0;
    return end != text && *end == '\0' && *speedup > 0;
}

static const struct {
    const char *name;
    int exp10;
} FLOPS_UNITS[] = {
    {"MFLOPS", 6}, {"GFLOPS", 9}, {"TFLOPS", 12},
    {"PFLOPS", 15}, {"EFLOPS", 18}, {"ZFLOPS", 21},
};

int flops_unit_count(void) {
    return (int)(sizeof FLOPS_UNITS / sizeof FLOPS_UNITS[0]);
}

const char *flops_unit_name(int index) {
    return FLOPS_UNITS[index].name;
}

int flops_unit_exp(int index) {
    return FLOPS_UNITS[index].exp10;
}

bool parse_flops_unit(const char *unit, int *exp10) {
    int i;
    if (unit == NULL) {
        return false;
    }
    for (i = 0; i < flops_unit_count(); i++) {
        if (eq_ci(unit, FLOPS_UNITS[i].name)) {
            *exp10 = FLOPS_UNITS[i].exp10;
            return true;
        }
    }
    return false;
}
