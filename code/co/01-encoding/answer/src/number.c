#include "number.h"

#include <stdio.h>
#include <string.h>

static void set_err(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
}

static unsigned long long gcd_u(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

static void reduce(Decimal *d) {
    unsigned long long g = gcd_u(d->num, d->den);
    d->num /= g;
    d->den /= g;
}

static bool take_decimal_digit(char c, int frac, unsigned long long *num, unsigned long long *den, char *err,
                               size_t errn) {
    if (c < '0' || c > '9') {
        set_err(err, errn, "十进制真值里只能有数字和至多一个小数点");
        return false;
    }
    if (*num > (~0ULL / 10)) {
        set_err(err, errn, "这个十进制数放不进 64 位");
        return false;
    }
    *num = *num * 10 + (unsigned)(c - '0');
    if (frac) {
        if (*den > (~0ULL / 10)) {
            set_err(err, errn, "小数位数太多");
            return false;
        }
        *den *= 10;
    }
    return true;
}

bool decimal_parse(const char *text, Decimal *out, char *err, size_t errn) {
    const char *p = text;
    unsigned long long num = 0;
    unsigned long long den = 1;
    int sign = 1;
    int saw_digit = 0;
    int frac = 0;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        p++;
    }
    if (*p == '\0') {
        set_err(err, errn, "缺少数值");
        return false;
    }
    while (*p != '\0') {
        if (*p == '.') {
            if (frac) {
                set_err(err, errn, "小数点多于一个");
                return false;
            }
            frac = 1;
            p++;
            continue;
        }
        if (!take_decimal_digit(*p, frac, &num, &den, err, errn)) {
            return false;
        }
        saw_digit = 1;
        p++;
    }
    if (!saw_digit) {
        set_err(err, errn, "缺少数值");
        return false;
    }
    out->sign = sign;
    out->num = num;
    out->den = den;
    out->minus_zero = (sign < 0 && num == 0);
    reduce(out);
    return true;
}

bool decimal_is_int(const Decimal *d) {
    return d->den == 1;
}

bool decimal_to_i64(const Decimal *d, long long *out) {
    if (d->den != 1 || d->num > 9223372036854775807ULL) {
        return false;
    }
    *out = d->sign < 0 ? -(long long)d->num : (long long)d->num;
    return true;
}

bool format_ratio(unsigned long long fn, unsigned long long fd, char *dst, size_t n) {
    unsigned long long whole;
    unsigned long long rem;
    char digits[16];
    int count = 0;
    int i;
    if (fd == 0 || n == 0) {
        return false;
    }
    whole = fn / fd;
    rem = fn % fd;
    if (rem == 0) {
        snprintf(dst, n, "%llu", whole);
        return true;
    }
    for (i = 0; i < 12 && rem != 0; i++) {
        if (rem > (~0ULL / 10)) {
            break;
        }
        rem *= 10;
        digits[count++] = (char)('0' + rem / fd);
        rem %= fd;
    }
    if (rem == 0) {
        int m = snprintf(dst, n, "%llu.", whole);
        if (m < 0 || (size_t)m >= n) {
            return false;
        }
        for (i = 0; i < count && (size_t)m + 1 < n; i++) {
            dst[m++] = digits[i];
        }
        dst[m] = '\0';
        return true;
    }
    snprintf(dst, n, "%llu/%llu", fn, fd);
    return true;
}
