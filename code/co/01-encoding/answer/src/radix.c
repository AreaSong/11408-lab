#include "radix.h"

#include <stdio.h>
#include <string.h>

static const char *DIGITS = "0123456789ABCDEF";

bool radix_base_ok(int base) {
    return base == 2 || base == 8 || base == 10 || base == 16;
}

const char *radix_name(int base) {
    if (base == 2) {
        return "二进制";
    }
    if (base == 8) {
        return "八进制";
    }
    if (base == 10) {
        return "十进制";
    }
    if (base == 16) {
        return "十六进制";
    }
    return "未知进制";
}

static int fail_msg(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
    return 1;
}

static int digit_of(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static unsigned long long gcd_u(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

static int take_radix_digit(int base, int d, int frac, unsigned long long *ip, unsigned long long *fn,
                            unsigned long long *fd, char *err, size_t errn) {
    if (!frac) {
        if (*ip > (~0ULL - (unsigned)d) / (unsigned)base) {
            return fail_msg(err, errn, "整数部分放不进 64 位");
        }
        *ip = *ip * (unsigned)base + (unsigned)d;
        return 0;
    }
    if (*fn > (~0ULL - (unsigned)d) / (unsigned)base || *fd > (~0ULL / (unsigned)base)) {
        return fail_msg(err, errn, "小数位数太多");
    }
    *fn = *fn * (unsigned)base + (unsigned)d;
    *fd *= (unsigned)base;
    return 0;
}

int radix_parse(int base, const char *text, RadixParts *out, char *err, size_t errn) {
    const char *p = text;
    unsigned long long ip = 0;
    unsigned long long fn = 0;
    unsigned long long fd = 1;
    int sign = 1;
    int saw = 0;
    int frac = 0;
    if (!radix_base_ok(base)) {
        return fail_msg(err, errn, "进制只能是 2、8、10、16");
    }
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        p++;
    }
    while (*p != '\0') {
        int d;
        if (*p == '.') {
            if (frac) {
                return fail_msg(err, errn, "小数点多于一个");
            }
            frac = 1;
            p++;
            continue;
        }
        d = digit_of(*p);
        if (d < 0 || d >= base) {
            snprintf(err, errn, "「%c」不是 %d 进制数字", *p, base);
            return 1;
        }
        if (take_radix_digit(base, d, frac, &ip, &fn, &fd, err, errn) != 0) {
            return 1;
        }
        saw = 1;
        p++;
    }
    if (!saw) {
        return fail_msg(err, errn, "缺少数值");
    }
    if (fn != 0) {
        unsigned long long g = gcd_u(fn, fd);
        fn /= g;
        fd /= g;
    }
    out->sign = (ip == 0 && fn == 0) ? 1 : sign;
    out->ip = ip;
    out->fn = fn;
    out->fd = fd;
    return 0;
}

static int write_int(unsigned long long x, int base, char *dst, int cap) {
    char rev[80];
    int n = 0;
    int i;
    if (x == 0) {
        if (cap < 2) {
            return -1;
        }
        dst[0] = '0';
        dst[1] = '\0';
        return 1;
    }
    while (x > 0 && n < (int)sizeof rev) {
        rev[n++] = DIGITS[x % (unsigned)base];
        x /= (unsigned)base;
    }
    if (x > 0 || n + 1 > cap) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        dst[i] = rev[n - 1 - i];
    }
    dst[n] = '\0';
    return n;
}

static int write_frac(unsigned long long fn, unsigned long long fd, int base, int max_digits, char *dst,
                      bool *exact) {
    int n = 0;
    *exact = true;
    if (fn == 0) {
        dst[0] = '\0';
        return 0;
    }
    while (fn != 0 && n < max_digits) {
        if (fn > (~0ULL / (unsigned)base)) {
            return -1;
        }
        fn *= (unsigned)base;
        dst[n++] = DIGITS[fn / fd];
        fn %= fd;
    }
    *exact = (fn == 0);
    while (n > 0 && dst[n - 1] == '0') {
        n--;
    }
    dst[n] = '\0';
    return n;
}

int radix_result(int from, int to, int frac_digits, const char *text, char *dst, size_t n, bool *exact,
                 char *err, size_t errn) {
    RadixParts part;
    char ibuf[80];
    char fbuf[80];
    int nf;
    bool frac_exact = true;
    if (!radix_base_ok(to) || frac_digits < 0 || frac_digits > 64) {
        return fail_msg(err, errn, "目标进制只能是 2、8、10、16，小数位数 0 到 64");
    }
    if (radix_parse(from, text, &part, err, errn) != 0) {
        return 1;
    }
    if (part.fn != 0 && frac_digits == 0) {
        return fail_msg(err, errn, "输入带小数，小数位数不能是 0");
    }
    if (write_int(part.ip, to, ibuf, (int)sizeof ibuf) < 0) {
        return fail_msg(err, errn, "整数部分转写失败");
    }
    nf = write_frac(part.fn, part.fd, to, frac_digits, fbuf, &frac_exact);
    if (nf < 0) {
        return fail_msg(err, errn, "小数部分的中间结果放不进 64 位");
    }
    if (nf > 0) {
        snprintf(dst, n, "%s%s.%s", part.sign < 0 ? "-" : "", ibuf, fbuf);
    } else {
        snprintf(dst, n, "%s%s", part.sign < 0 ? "-" : "", ibuf);
    }
    *exact = frac_exact;
    return 0;
}
