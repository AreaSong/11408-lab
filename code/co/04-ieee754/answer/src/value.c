#include "fp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fp_hex(uint64_t bits, int digits, char *buf) {
    static const char hex[] = "0123456789ABCDEF";
    int i;
    for (i = 0; i < digits; i++) {
        int shift = (digits - 1 - i) * 4;
        buf[i] = hex[(bits >> shift) & 0xF];
    }
    buf[digits] = 'H';
    buf[digits + 1] = '\0';
}

const char *fp_kind_name(Kind kind) {
    switch (kind) {
    case KIND_ZERO:
        return "零";
    case KIND_SUB:
        return "非规格化数";
    case KIND_NORM:
        return "规格化数";
    case KIND_INF:
        return "无穷";
    case KIND_NAN:
        return "NaN";
    }
    return "未知";
}

View fp_view(uint64_t bits, const Fmt *fmt) {
    View v;
    int fb = fmt->frac_bits;
    int eb = fmt->exp_bits;
    int exp_max = (1 << eb) - 1;
    uint64_t frac_mask = (1ULL << fb) - 1ULL;
    memset(&v, 0, sizeof v);
    if (eb + fb < 63) {
        bits &= (1ULL << (eb + fb + 1)) - 1ULL;
    }
    v.sign = (int)((bits >> (eb + fb)) & 1ULL);
    v.exp_field = (int)((bits >> fb) & (uint64_t)exp_max);
    v.frac = bits & frac_mask;
    if (v.exp_field == 0) {
        v.kind = (v.frac == 0) ? KIND_ZERO : KIND_SUB;
        v.exp_true = 1 - fmt->bias;
        v.sig = v.frac;
    } else if (v.exp_field == exp_max) {
        v.kind = (v.frac == 0) ? KIND_INF : KIND_NAN;
    } else {
        v.kind = KIND_NORM;
        v.exp_true = v.exp_field - fmt->bias;
        v.sig = ((uint64_t)1 << fb) | v.frac;
    }
    return v;
}

static void write_pow(char *buf, size_t n, int exp) {
    if (exp < 0) {
        snprintf(buf, n, "2^(%d)", exp);
    } else {
        snprintf(buf, n, "2^%d", exp);
    }
}

static int pow2_exp(const View *v, const Fmt *fmt, int *exp) {
    int fb = fmt->frac_bits;
    if (v->kind == KIND_NORM && v->frac == 0) {
        *exp = v->exp_true;
        return 1;
    }
    if (v->kind == KIND_SUB && v->frac != 0 && (v->frac & (v->frac - 1)) == 0) {
        int shift = 0;
        uint64_t m = v->frac;
        while (m > 1) {
            m >>= 1;
            shift++;
        }
        *exp = v->exp_true - fb + shift;
        return 1;
    }
    return 0;
}

static int mant_digits(uint64_t frac, int fb, int hidden, char *buf, size_t n) {
    char digs[12];
    int nd = 0;
    uint64_t rem = frac;
    uint64_t unit = 1ULL << fb;
    int i;
    while (rem != 0 && nd < 8) {
        uint64_t prod = rem * 10ULL;
        digs[nd++] = (char)('0' + prod / unit);
        rem = prod % unit;
    }
    if (rem != 0) {
        return 0;
    }
    while (nd > 0 && digs[nd - 1] == '0') {
        nd--;
    }
    if (nd == 0) {
        snprintf(buf, n, "%d.0", hidden);
        return 1;
    }
    buf[0] = (char)('0' + hidden);
    buf[1] = '.';
    for (i = 0; i < nd; i++) {
        buf[2 + i] = digs[i];
    }
    buf[2 + nd] = '\0';
    return 1;
}

static int bin_mant(const View *v, const Fmt *fmt, char *buf, size_t n) {
    int fb = fmt->frac_bits;
    int hidden = (v->kind == KIND_NORM);
    int show = fb < 12 ? fb : 12;
    int i;
    int p = 0;
    char tmp[32];
    tmp[p++] = hidden ? '1' : '0';
    tmp[p++] = '.';
    for (i = 0; i < show; i++) {
        int bit = (int)((v->frac >> (fb - 1 - i)) & 1ULL);
        tmp[p++] = bit ? '1' : '0';
    }
    if (show < fb) {
        tmp[p++] = '.';
        tmp[p++] = '.';
        tmp[p++] = '.';
    }
    tmp[p++] = 'B';
    tmp[p] = '\0';
    snprintf(buf, n, "%s", tmp);
    return 1;
}

int fp_exam(const View *v, const Fmt *fmt, char *buf, size_t n) {
    char mant[24];
    char pow[24];
    int exp = 0;
    const char *sign = v->sign ? "-" : "+";
    if (v->kind == KIND_NAN) {
        snprintf(buf, n, "NaN");
        return 1;
    }
    if (v->kind == KIND_INF) {
        snprintf(buf, n, "%s∞", sign);
        return 1;
    }
    if (v->kind == KIND_ZERO) {
        snprintf(buf, n, "%s0", sign);
        return 1;
    }
    if (v->kind == KIND_NORM && v->frac == 0) {
        write_pow(pow, sizeof pow, v->exp_true);
        snprintf(buf, n, "%s1.0×%s", sign, pow);
        return 1;
    }
    if (pow2_exp(v, fmt, &exp)) {
        write_pow(pow, sizeof pow, exp);
        snprintf(buf, n, "%s%s", sign, pow);
        return 1;
    }
    write_pow(pow, sizeof pow, v->exp_true);
    if (mant_digits(v->frac, fmt->frac_bits, v->kind == KIND_NORM, mant, sizeof mant)) {
        snprintf(buf, n, "%s%s×%s", sign, mant, pow);
        return 1;
    }
    bin_mant(v, fmt, mant, sizeof mant);
    snprintf(buf, n, "%s%s×%s", sign, mant, pow);
    return 1;
}

static int append_frac(uint64_t rem, int rsh, char *buf, size_t n, size_t used) {
    char digs[16];
    int nd = 0;
    uint64_t unit;
    if (rsh < 0 || rsh > 53) {
        return 0;
    }
    unit = 1ULL << rsh;
    while (rem != 0 && nd < 12) {
        uint64_t prod = rem * 10ULL;
        digs[nd++] = (char)('0' + (prod / unit));
        rem = prod % unit;
    }
    if (rem != 0) {
        return 0;
    }
    while (nd > 0 && digs[nd - 1] == '0') {
        nd--;
    }
    if (nd == 0) {
        return 1;
    }
    if (used + 1 + (size_t)nd + 1 >= n) {
        return 0;
    }
    buf[used++] = '.';
    memcpy(buf + used, digs, (size_t)nd);
    buf[used + (size_t)nd] = '\0';
    return 1;
}

int fp_decimal(const View *v, const Fmt *fmt, char *buf, size_t n) {
    int fb = fmt->frac_bits;
    int shift;
    uint64_t mag;
    const char *sign = v->sign ? "-" : "+";
    if (v->kind == KIND_ZERO) {
        snprintf(buf, n, "%s0", sign);
        return 1;
    }
    if (v->kind != KIND_NORM && v->kind != KIND_SUB) {
        return 0;
    }
    mag = (v->kind == KIND_NORM) ? v->sig : v->frac;
    shift = v->exp_true - fb;
    if (shift >= 0) {
        if (shift > 40 || (shift > 0 && mag > (UINT64_MAX >> shift))) {
            return 0;
        }
        snprintf(buf, n, "%s%llu", sign, (unsigned long long)(mag << shift));
        return 1;
    }
    {
        int rsh = -shift;
        uint64_t ip;
        uint64_t rem;
        int wrote;
        if (rsh > 53) {
            return 0;
        }
        ip = mag >> rsh;
        rem = mag & ((1ULL << rsh) - 1ULL);
        wrote = snprintf(buf, n, "%s%llu", sign, (unsigned long long)ip);
        if (wrote < 0 || (size_t)wrote >= n) {
            return 0;
        }
        return append_frac(rem, rsh, buf, n, (size_t)wrote);
    }
}

uint32_t c_float_bits(const char *text) {
    union {
        float f;
        uint32_t u;
    } box;
    box.f = strtof(text, NULL);
    return box.u;
}

uint64_t c_double_bits(const char *text) {
    union {
        double d;
        uint64_t u;
    } box;
    box.d = strtod(text, NULL);
    return box.u;
}

uint32_t c_fadd(uint32_t a, uint32_t b) {
    union {
        float f;
        uint32_t u;
    } x, y, z;
    x.u = a;
    y.u = b;
    z.f = x.f + y.f;
    return z.u;
}
