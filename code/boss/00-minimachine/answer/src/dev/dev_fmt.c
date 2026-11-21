#include "dev/dev_fmt.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int eq_ci(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static double snap(double x) {
    double nearest;
    double ax;
    char tmp[64];
    if (!isfinite(x) || x == 0) {
        return x;
    }
    nearest = round(x);
    if (fabs(nearest) >= 1.0 && fabs(x - nearest) < 1e-4 && fabs(nearest) < 1e15) {
        return nearest;
    }
    ax = fabs(x);
    if (ax < 1e-4 || ax >= 1e15) {
        snprintf(tmp, sizeof tmp, "%.6g", x);
        return strtod(tmp, NULL);
    }
    return round(x * 1e6) / 1e6;
}

static void strip_zeros(char *buf) {
    char *dot = strchr(buf, '.');
    char *end;
    if (dot == NULL) {
        return;
    }
    end = buf + strlen(buf) - 1;
    while (end > dot && *end == '0') {
        *end-- = '\0';
    }
    if (end == dot) {
        *end = '\0';
    }
}

void dev_fmt_num(char *buf, size_t n, double x) {
    double y = snap(x);
    double ay = fabs(y);
    if (!isfinite(y)) {
        snprintf(buf, n, "nan");
        return;
    }
    if (y == 0 || (ay >= 1.0 && fabs(y - round(y)) < 1e-6 && ay < 1e15)) {
        snprintf(buf, n, "%.0f", y);
        return;
    }
    if (ay >= 1e15 || ay < 1e-4) {
        snprintf(buf, n, "%.6g", y);
        return;
    }
    snprintf(buf, n, "%.6f", y);
    strip_zeros(buf);
}

static int scale_of(double value, const char *unit, const char *const *names, const double *scales,
                    int count, double *out) {
    int i;
    if (unit == NULL || !(value > 0)) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        double raw;
        if (!eq_ci(unit, names[i])) {
            continue;
        }
        raw = value * scales[i];
        if (fabs(raw - round(raw)) < 1e-4 && fabs(round(raw)) < 1e15) {
            raw = round(raw);
        }
        *out = raw;
        return 1;
    }
    return 0;
}

int dev_parse_hz(double value, const char *unit, double *hz) {
    static const char *names[] = {"Hz", "kHz", "MHz", "GHz"};
    static const double scales[] = {1, 1e3, 1e6, 1e9};
    return scale_of(value, unit, names, scales, 4, hz);
}

int dev_parse_rate(double value, const char *unit, double *bps) {
    static const char *names[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    static const double scales[] = {1, 1e3, 1e6, 1e9};
    return scale_of(value, unit, names, scales, 4, bps);
}

int dev_parse_seconds(double value, const char *unit, double *seconds) {
    static const char *names[] = {"s", "ms", "us", "ns"};
    static const double scales[] = {1, 1e-3, 1e-6, 1e-9};
    int i;
    if (unit == NULL || value < 0) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        double raw;
        double ps;
        if (!eq_ci(unit, names[i])) {
            continue;
        }
        raw = value * scales[i];
        ps = raw * 1e12;
        if (fabs(ps) < 1e15 && fabs(ps - round(ps)) < 0.01) {
            raw = round(ps) / 1e12;
        }
        *seconds = raw;
        return 1;
    }
    return 0;
}

void dev_fmt_rate(char *buf, size_t n, double bytes_per_s) {
    char num[64];
    if (snap(bytes_per_s / 1e9) >= 1.0) {
        dev_fmt_num(num, sizeof num, bytes_per_s / 1e9);
        snprintf(buf, n, "%s GB/s", num);
        return;
    }
    if (snap(bytes_per_s / 1e6) >= 1.0) {
        dev_fmt_num(num, sizeof num, bytes_per_s / 1e6);
        snprintf(buf, n, "%s MB/s", num);
        return;
    }
    dev_fmt_num(num, sizeof num, bytes_per_s);
    snprintf(buf, n, "%s B/s", num);
}

void dev_fmt_time(char *buf, size_t n, double seconds) {
    char num[64];
    double ax = fabs(seconds);
    const char *unit = "s";
    double shown = seconds;
    if (ax < 1e-7) {
        unit = "ns";
        shown = seconds * 1e9;
    } else if (ax < 1e-4) {
        unit = "us";
        shown = seconds * 1e6;
    } else if (ax < 1) {
        unit = "ms";
        shown = seconds * 1e3;
    }
    dev_fmt_num(num, sizeof num, shown);
    snprintf(buf, n, "%s %s", num, unit);
}

void dev_fmt_pct(char *buf, size_t n, double ratio) {
    char num[64];
    dev_fmt_num(num, sizeof num, ratio * 100.0);
    snprintf(buf, n, "%s%%", num);
}
