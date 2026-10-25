#include "unit.h"

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

static bool scale_of(const char *unit, const char *const *names, const uint64_t *scales,
                     int count, uint64_t *scale) {
    int i;
    for (i = 0; i < count; i++) {
        if (eq_ci(unit, names[i])) {
            *scale = scales[i];
            return true;
        }
    }
    return false;
}

bool parse_bin(double value, const char *unit, uint64_t *out) {
    static const char *names[] = {"B", "K", "KB", "M", "MB", "G", "GB", "T", "TB"};
    static const uint64_t scales[] = {1ull, 1ull << 10, 1ull << 10, 1ull << 20, 1ull << 20,
                                       1ull << 30, 1ull << 30, 1ull << 40, 1ull << 40};
    uint64_t scale = 1;
    double raw;
    if (!(value > 0) || unit == NULL || !scale_of(unit, names, scales, 9, &scale)) {
        return false;
    }
    raw = value * (double)scale;
    if (fabs(raw - round(raw)) > 1e-4 || raw > (double)UINT64_MAX) {
        return false;
    }
    *out = (uint64_t)llround(raw);
    return *out > 0;
}

bool parse_dec_bytes(double value, const char *unit, double *out) {
    static const char *names[] = {"B", "K", "KB", "M", "MB", "G", "GB", "T", "TB"};
    static const double scales[] = {1, 1e3, 1e3, 1e6, 1e6, 1e9, 1e9, 1e12, 1e12};
    int i;
    if (!(value > 0) || unit == NULL) {
        return false;
    }
    for (i = 0; i < 9; i++) {
        if (eq_ci(unit, names[i])) {
            *out = value * scales[i];
            return true;
        }
    }
    return false;
}

bool parse_rate(double value, const char *unit, double *bytes_per_s) {
    static const char *names[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    static const double scales[] = {1, 1e3, 1e6, 1e9, 1e12};
    int i;
    if (!(value > 0) || unit == NULL) {
        return false;
    }
    for (i = 0; i < 5; i++) {
        if (eq_ci(unit, names[i])) {
            *bytes_per_s = value * scales[i];
            return true;
        }
    }
    return false;
}

bool parse_hz(double value, const char *unit, double *hz) {
    static const char *names[] = {"Hz", "kHz", "MHz", "GHz"};
    static const double scales[] = {1, 1e3, 1e6, 1e9};
    int i;
    if (!(value > 0) || unit == NULL) {
        return false;
    }
    for (i = 0; i < 4; i++) {
        if (eq_ci(unit, names[i])) {
            *hz = value * scales[i];
            return true;
        }
    }
    return false;
}

bool parse_seconds(double value, const char *unit, double *seconds) {
    static const char *names[] = {"s", "ms", "us", "ns"};
    static const double scales[] = {1, 1e-3, 1e-6, 1e-9};
    int i;
    if (!(value >= 0) || unit == NULL) {
        return false;
    }
    for (i = 0; i < 4; i++) {
        if (eq_ci(unit, names[i])) {
            *seconds = value * scales[i];
            return true;
        }
    }
    return false;
}

bool parse_addr(const char *text, uint64_t *addr) {
    char buf[32];
    char *end = NULL;
    size_t n;
    if (text == NULL) {
        return false;
    }
    n = strlen(text);
    if (n == 0 || n >= sizeof buf) {
        return false;
    }
    memcpy(buf, text, n + 1);
    if (n > 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
        *addr = strtoull(buf, &end, 16);
        return end != buf && *end == '\0';
    }
    if (buf[n - 1] == 'H' || buf[n - 1] == 'h') {
        buf[n - 1] = '\0';
        *addr = strtoull(buf, &end, 16);
        return buf[0] != '\0' && end != buf && *end == '\0';
    }
    return false;
}

int exact_log2_u64(uint64_t x) {
    int bits = 0;
    if (x == 0 || (x & (x - 1)) != 0) {
        return -1;
    }
    while (x > 1) {
        x >>= 1;
        bits++;
    }
    return bits;
}

int ceil_log2_u64(uint64_t x) {
    int bits = 0;
    uint64_t value = 1;
    if (x == 0) {
        return -1;
    }
    while (value < x) {
        if (bits >= 63) {
            return -1;
        }
        value <<= 1;
        bits++;
    }
    return bits;
}

void fmt_time_unit(char *buf, size_t n, double seconds, const char *unit) {
    double scale = 1;
    char num[64];
    if (eq_ci(unit, "ms")) {
        scale = 1e3;
    } else if (eq_ci(unit, "us")) {
        scale = 1e6;
    } else if (eq_ci(unit, "ns")) {
        scale = 1e9;
    }
    fmt_num(num, sizeof num, seconds * scale);
    snprintf(buf, n, "%s %s", num, unit);
}

void fmt_bin_count(char *buf, size_t n, uint64_t count) {
    static const char *names[] = {"K", "M", "G", "T"};
    int shift;
    int bits = exact_log2_u64(count);
    for (shift = 4; shift >= 1; shift--) {
        uint64_t scale = 1ull << (10 * shift);
        if (count >= scale && count % scale == 0) {
            if (bits >= 0) {
                snprintf(buf, n, "%llu = %llu %s = 2^%d", (unsigned long long)count,
                         (unsigned long long)(count / scale), names[shift - 1], bits);
            } else {
                snprintf(buf, n, "%llu = %llu %s", (unsigned long long)count,
                         (unsigned long long)(count / scale), names[shift - 1]);
            }
            return;
        }
    }
    if (bits >= 0) {
        snprintf(buf, n, "%llu = 2^%d", (unsigned long long)count, bits);
        return;
    }
    snprintf(buf, n, "%llu", (unsigned long long)count);
}

void fmt_hex(char *buf, size_t n, uint64_t value) {
    snprintf(buf, n, "%04llXH", (unsigned long long)value);
}

void fmt_hex_pad(char *buf, size_t n, uint64_t value, int digits) {
    if (digits < 4) {
        digits = 4;
    }
    if (digits > 16) {
        digits = 16;
    }
    snprintf(buf, n, "%0*llXH", digits, (unsigned long long)value);
}

void fmt_bits(char *buf, size_t n, uint64_t value, int bits) {
    int i;
    if (bits <= 0 || (size_t)bits + 1 > n) {
        snprintf(buf, n, "?");
        return;
    }
    for (i = bits - 1; i >= 0; i--) {
        buf[bits - 1 - i] = ((value >> i) & 1ull) ? '1' : '0';
    }
    buf[bits] = '\0';
}
