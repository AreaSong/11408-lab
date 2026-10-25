#include "frac.h"

#include <stdio.h>
#include <string.h>

void frac_err(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
}

bool frac_same(Frac x, Frac y, char *err, size_t errn) {
    if (x.n != y.n) {
        frac_err(err, errn, "两个数的数值位数要相同");
        return false;
    }
    return true;
}

long long frac_num(Frac f) {
    if (f.mag == 0) {
        return 0;
    }
    return f.sign ? -(long long)f.mag : (long long)f.mag;
}

bool frac_parse(const char *text, Frac *out, char *err, size_t errn) {
    int sign = 0;
    int n;
    int i;
    unsigned mag = 0;
    if (text[0] == '+' || text[0] == '-') {
        sign = text[0] == '-';
        text++;
    }
    if (text[0] != '0' || text[1] != '.') {
        frac_err(err, errn, "定点小数写成 +0.1101 这样，小数点前是 0");
        return false;
    }
    text += 2;
    n = (int)strlen(text);
    if (n < 1 || n > FRAC_MAX) {
        frac_err(err, errn, "小数点后写 1 到 8 位 0 或 1");
        return false;
    }
    for (i = 0; i < n; i++) {
        if (text[i] != '0' && text[i] != '1') {
            frac_err(err, errn, "小数点后只能是 0 或 1");
            return false;
        }
        mag = (mag << 1) | (unsigned)(text[i] - '0');
    }
    out->n = n;
    out->sign = sign;
    out->mag = mag;
    return true;
}

void frac_plain(char *dst, size_t n, unsigned bits, int width) {
    int i;
    if (n < (size_t)width + 1 || width < 0 || width > 40) {
        if (n > 0) {
            dst[0] = '\0';
        }
        return;
    }
    for (i = 0; i < width; i++) {
        dst[i] = ((bits >> (width - 1 - i)) & 1u) ? '1' : '0';
    }
    dst[width] = '\0';
}

void frac_dot(char *dst, size_t n, unsigned bits, int left, int mag) {
    char raw[48];
    int width = left + mag;
    if (left < 0 || mag < 0 || n < (size_t)width + 2) {
        if (n > 0) {
            dst[0] = '\0';
        }
        return;
    }
    frac_plain(raw, sizeof raw, bits, width);
    memcpy(dst, raw, (size_t)left);
    dst[left] = '.';
    memcpy(dst + left + 1, raw + left, (size_t)mag + 1);
}

void frac_text(char *dst, size_t n, Frac f) {
    char bits[16];
    frac_plain(bits, sizeof bits, f.mag, f.n);
    snprintf(dst, n, "%s0.%s", f.sign ? "-" : "+", bits);
}

static long long iabs64(long long v) {
    return v < 0 ? -v : v;
}

static long long gcd64(long long a, long long b) {
    a = iabs64(a);
    b = iabs64(b);
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

void frac_ratio(char *dst, size_t n, long long num, int den_bits) {
    long long den = 1LL << den_bits;
    long long g;
    int neg = num < 0;
    if (num == 0) {
        snprintf(dst, n, "0");
        return;
    }
    g = gcd64(num, den);
    num = iabs64(num) / g;
    den /= g;
    snprintf(dst, n, "%s%lld/%lld", neg ? "-" : "+", num, den);
}
