#include "fp.h"

#include <stdio.h>

static int fail(char *err, size_t n, const char *msg) {
    snprintf(err, n, "%s", msg);
    return 0;
}

static int exp_part(const char **pp, int *extra, char *err, size_t n) {
    const char *p = *pp;
    int sign = 1;
    int value = 0;
    int any = 0;
    if (*p != 'e' && *p != 'E') {
        return 1;
    }
    p++;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        any = 1;
        if (value > 100000) {
            return fail(err, n, "十进制指数太大");
        }
        value = value * 10 + (*p - '0');
        p++;
    }
    if (!any || *p != '\0') {
        return fail(err, n, "科学计数法的指数写完才能结束");
    }
    *extra = sign * value;
    *pp = p;
    return 1;
}

static int read_coef(const char **pp, uint64_t *coef, int *frac, char *err, size_t n) {
    const char *p = *pp;
    int saw_dot = 0;
    int saw_digit = 0;
    int lead = 1;
    int significant = 0;
    *coef = 0;
    *frac = 0;
    while (*p != '\0' && *p != 'e' && *p != 'E') {
        if (*p == '.') {
            if (saw_dot) {
                return fail(err, n, "小数点只能有一个");
            }
            saw_dot = 1;
            p++;
            continue;
        }
        if (*p < '0' || *p > '9') {
            return fail(err, n, "十进制里只能有数字、一个小数点和科学计数法");
        }
        saw_digit = 1;
        if (saw_dot) {
            (*frac)++;
        }
        if (lead && *p == '0') {
            p++;
            continue;
        }
        lead = 0;
        if (significant >= 18) {
            return fail(err, n, "有效数字超过 18 位，这个关卡只手算更短的数");
        }
        *coef = *coef * 10 + (uint64_t)(*p - '0');
        significant++;
        p++;
    }
    if (!saw_digit) {
        return fail(err, n, "缺少数字");
    }
    *pp = p;
    return 1;
}

int dec_parse(const char *text, Decimal *out, char *err, size_t n) {
    const char *p = text;
    uint64_t coef = 0;
    int sign = 0;
    int frac = 0;
    int extra = 0;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-');
        p++;
    }
    if (!read_coef(&p, &coef, &frac, err, n)) {
        return 0;
    }
    if (!exp_part(&p, &extra, err, n)) {
        return 0;
    }
    if (*p != '\0') {
        return fail(err, n, "这一项后面还有多余字符");
    }
    out->sign = sign;
    out->coef = coef;
    out->exp10 = extra - frac;
    if (coef != 0 && (out->exp10 > 45 || out->exp10 < -45)) {
        return fail(err, n, "十进制指数请落在 -45 到 45 之间");
    }
    return 1;
}

static int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

int hex_parse(const char *text, int digits, uint64_t *out, char *err, size_t n) {
    const char *p = text;
    uint64_t value = 0;
    int count = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    if (*p == '\0') {
        return fail(err, n, "缺少十六进制数字");
    }
    while (*p != '\0' && *p != 'H' && *p != 'h') {
        int d = hex_digit(*p);
        if (d < 0) {
            return fail(err, n, "机器数写成 C1040000H 或 0xC1040000");
        }
        if (count >= digits) {
            return fail(err, n, "十六进制位数超出这一种浮点格式");
        }
        value = (value << 4) | (uint64_t)d;
        count++;
        p++;
    }
    if (*p == 'H' || *p == 'h') {
        p++;
    }
    if (*p != '\0' || count == 0) {
        return fail(err, n, "机器数写成 C1040000H 或 0xC1040000");
    }
    *out = value;
    return 1;
}
