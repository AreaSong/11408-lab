#include "show.h"

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

static bool scale_named(double value, const char *unit, const char *const *names,
                        const double *scales, int count, double *out, int nonnegative) {
    int i;
    if (unit == NULL || (nonnegative ? value < 0 : !(value > 0))) {
        return false;
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
        return true;
    }
    return false;
}

bool parse_hz(double value, const char *unit, double *hz) {
    static const char *names[] = {"Hz", "kHz", "MHz", "GHz"};
    static const double scales[] = {1, 1e3, 1e6, 1e9};
    return scale_named(value, unit, names, scales, 4, hz, 0);
}

bool parse_seconds(double value, const char *unit, double *seconds) {
    static const char *names[] = {"s", "ms", "us", "ns"};
    static const double scales[] = {1, 1e-3, 1e-6, 1e-9};
    int i;
    if (unit == NULL || value < 0) {
        return false;
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
        return true;
    }
    return false;
}

bool parse_rate(double value, const char *unit, double *bytes_per_s) {
    static const char *names[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    static const double scales[] = {1, 1e3, 1e6, 1e9};
    return scale_named(value, unit, names, scales, 4, bytes_per_s, 0);
}

void fmt_hz(char *buf, size_t n, double hz) {
    char h[64];
    char m[64];
    char g[64];
    fmt_num(h, sizeof h, hz);
    fmt_num(m, sizeof m, hz / 1e6);
    fmt_num(g, sizeof g, hz / 1e9);
    snprintf(buf, n, "%s Hz = %s MHz = %s GHz", h, m, g);
}

void fmt_seconds(char *buf, size_t n, double seconds) {
    char base[64];
    char num[64];
    fmt_num(base, sizeof base, seconds);
    fmt_time_short(num, sizeof num, seconds);
    if (strcmp(num, "0 s") == 0 || seconds >= 0.1) {
        snprintf(buf, n, "%s s", base);
        return;
    }
    snprintf(buf, n, "%s s = %s", base, num);
}

void fmt_rate(char *buf, size_t n, double bytes_per_s) {
    char b[64];
    char mb[64];
    char gb[64];
    fmt_num(b, sizeof b, bytes_per_s);
    fmt_num(mb, sizeof mb, bytes_per_s / 1e6);
    fmt_num(gb, sizeof gb, bytes_per_s / 1e9);
    if (bytes_per_s >= 1e9) {
        snprintf(buf, n, "%s B/s = %s MB/s = %s GB/s", b, mb, gb);
        return;
    }
    snprintf(buf, n, "%s B/s = %s MB/s", b, mb);
}

void fmt_rate_short(char *buf, size_t n, double bytes_per_s) {
    char num[64];
    /* 差一丁点不到 1 GB/s 时，按 MB/s 会印成 1000，和 1 GB/s 是同一个数 */
    if (snap(bytes_per_s / 1e9) >= 1.0) {
        fmt_num(num, sizeof num, bytes_per_s / 1e9);
        snprintf(buf, n, "%s GB/s", num);
        return;
    }
    if (snap(bytes_per_s / 1e6) >= 1.0) {
        fmt_num(num, sizeof num, bytes_per_s / 1e6);
        snprintf(buf, n, "%s MB/s", num);
        return;
    }
    fmt_num(num, sizeof num, bytes_per_s);
    snprintf(buf, n, "%s B/s", num);
}

void fmt_time_short(char *buf, size_t n, double seconds) {
    char num[64];
    double ax = fabs(seconds);
    const char *unit = "s";
    double shown = seconds;
    /* 0.5 ms 不要印成 500 us，0.1 us 以上才用 us，再小用 ns */
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
    fmt_num(num, sizeof num, shown);
    snprintf(buf, n, "%s %s", num, unit);
}

void fmt_percent(char *buf, size_t n, double ratio) {
    char num[64];
    fmt_num(num, sizeof num, ratio * 100.0);
    snprintf(buf, n, "%s%%", num);
}
