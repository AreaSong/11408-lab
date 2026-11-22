/*
 * 补码浮点加减。考纲的五步还在：对阶、尾数相加、规格化、舍入、判溢出。
 * 和 IEEE 754 的差别是阶码、尾数都用补码，各含 2 位符号位，尾数没有隐含 1。
 * 规格化看双符号位：00.1 和 11.0 才算规格化；01、10 右规；00.0、11.1 左规。
 * 阶码双符号位变成 01 是上溢，10 是下溢。2009-13 右规之后阶码从 7 变成 8，就是上溢。
 */
#include "nfp.h"

#include <string.h>

static int width_mask(int width) {
    return (int)((1u << width) - 1u);
}

int nfp_low(int value, int width) {
    return (int)((unsigned)value & (unsigned)width_mask(width));
}

int nfp_signed(int bits, int width) {
    int masked = bits & width_mask(width);
    int sign = 1 << (width - 1);
    if (masked & sign) {
        return masked - (sign << 1);
    }
    return masked;
}

int nfp_shape(int frac, int width) {
    int bits = nfp_low(frac, width);
    int s2;
    int mag;
    if (width < 3 || bits == 0) {
        return bits == 0 ? NFP_ZERO : NFP_LEFT;
    }
    s2 = (bits >> (width - 2)) & 3;
    mag = (bits >> (width - 3)) & 1;
    if (s2 == 1 || s2 == 2) {
        return NFP_RIGHT;
    }
    if ((s2 == 0 && mag == 1) || (s2 == 3 && mag == 0)) {
        return NFP_NORM;
    }
    return NFP_LEFT;
}

static int exp_max(int exp_bits) {
    return (1 << (exp_bits - 2)) - 1;
}

static int exp_min(int exp_bits) {
    return -(1 << (exp_bits - 2));
}

static int width_ok(int exp_bits, int frac_bits) {
    return exp_bits >= 3 && exp_bits <= 8 && frac_bits >= 3 && frac_bits <= 16;
}

/* 补码右移补符号。对阶和右规都要这样，逻辑右移会把负数的符号位挪丢。 */
static int arith_shr1(int value, int width) {
    int bits = nfp_low(value, width);
    int sign = (bits >> (width - 1)) & 1;
    bits = (bits >> 1) | (sign << (width - 1));
    return nfp_signed(bits, width);
}

static int shl1(int value, int width) {
    int bits = (nfp_low(value, width) << 1) & width_mask(width);
    return nfp_signed(bits, width);
}

int nfp_from_fraction(NfpNum *out, int exp_bits, int frac_bits, int exp, int numer, int denom) {
    int mag;
    int scale;
    long long prod;
    if (out == NULL || !width_ok(exp_bits, frac_bits) || denom == 0) {
        return 1;
    }
    if (exp < exp_min(exp_bits) || exp > exp_max(exp_bits)) {
        return 2;
    }
    if (denom < 0) {
        numer = -numer;
        denom = -denom;
    }
    mag = frac_bits - 2;
    scale = 1 << mag;
    prod = (long long)numer * scale;
    if (prod % denom != 0) {
        return 3;
    }
    prod /= denom;
    /* 2 位符号位能表示的尾数整数是 -2^mag 到 2^mag-1，01、10 开头的不算能存的数。 */
    if (prod < -(long long)scale || prod > (long long)scale - 1) {
        return 4;
    }
    out->exp_bits = exp_bits;
    out->frac_bits = frac_bits;
    out->exp = exp;
    out->frac = (int)prod;
    out->overflow = 0;
    return 0;
}

static int parse_field(const char *text, int width, int *value) {
    int i;
    int bits = 0;
    if (text == NULL || (int)strlen(text) != width) {
        return 0;
    }
    for (i = 0; i < width; i++) {
        if (text[i] != '0' && text[i] != '1') {
            return 0;
        }
        bits = (bits << 1) | (text[i] - '0');
    }
    *value = nfp_signed(bits, width);
    return 1;
}

int nfp_from_bits(NfpNum *out, int exp_bits, int frac_bits, const char *exp_text, const char *frac_text) {
    int exp = 0;
    int frac = 0;
    if (out == NULL || !width_ok(exp_bits, frac_bits)) {
        return 1;
    }
    if (!parse_field(exp_text, exp_bits, &exp) || !parse_field(frac_text, frac_bits, &frac)) {
        return 1;
    }
    out->exp_bits = exp_bits;
    out->frac_bits = frac_bits;
    out->exp = exp;
    out->frac = frac;
    out->overflow = 0;
    if (exp > exp_max(exp_bits)) {
        out->overflow = 1;
    } else if (exp < exp_min(exp_bits)) {
        out->overflow = -1;
    }
    return 0;
}

static int shift_right(int frac, int width, int steps, unsigned *lost, int *any) {
    int i;
    int cur = frac;
    unsigned bits = 0;
    *any = 0;
    for (i = 0; i < steps; i++) {
        int bit = cur & 1;
        if (bit) {
            *any = 1;
        }
        if (i < 12) {
            bits = (bits << 1) | (unsigned)bit;
        }
        cur = arith_shr1(cur, width);
    }
    *lost = bits;
    return cur;
}

static void normalize(NfpAdd *out) {
    int width = out->x.frac_bits;
    int frac = out->sum_frac;
    int exp = out->sum_exp;
    int lost = 0;
    int n = 0;
    int shape = nfp_shape(frac, width);
    out->norm_dir = 0;
    if (shape == NFP_RIGHT) {
        out->norm_dir = 1;
    } else if (shape == NFP_LEFT) {
        out->norm_dir = -1;
    }
    while (n < width && nfp_shape(frac, width) == NFP_RIGHT) {
        lost = (lost << 1) | (frac & 1);
        frac = arith_shr1(frac, width);
        exp += 1;
        n++;
    }
    while (n < width && nfp_shape(frac, width) == NFP_LEFT) {
        frac = shl1(frac, width);
        exp -= 1;
        n++;
    }
    out->norm_n = n;
    out->norm_lost = lost;
    out->result_frac = frac;
    out->result_exp = frac == 0 ? 0 : exp;
    out->overflow = 0;
    if (frac == 0) {
        return;
    }
    if (exp > exp_max(out->x.exp_bits)) {
        out->overflow = 1;
    } else if (exp < exp_min(out->x.exp_bits)) {
        out->overflow = -1;
    }
}

int nfp_add(NfpAdd *out, const NfpNum *a, const NfpNum *b) {
    int width;
    int small;
    int kept;
    int lim;
    if (out == NULL || a == NULL || b == NULL || a->overflow || b->overflow) {
        return 1;
    }
    if (a->exp_bits != b->exp_bits || a->frac_bits != b->frac_bits || !width_ok(a->exp_bits, a->frac_bits)) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    out->x = *a;
    out->y = *b;
    width = a->frac_bits;
    if (a->exp >= b->exp) {
        out->y_small = 1;
        out->diff = a->exp - b->exp;
        small = b->frac;
        kept = a->frac;
        out->sum_exp = a->exp;
    } else {
        out->y_small = 0;
        out->diff = b->exp - a->exp;
        small = a->frac;
        kept = b->frac;
        out->sum_exp = b->exp;
    }
    if (out->diff > 31) {
        return 1;
    }
    out->aligned_frac = shift_right(small, width, out->diff, &out->align_lost, &out->align_any);
    out->align_show = out->diff > 12 ? 12 : out->diff;
    out->sum_frac = kept + out->aligned_frac;
    lim = 1 << (width - 1);
    if (out->sum_frac < -lim || out->sum_frac > lim - 1) {
        return 1;
    }
    normalize(out);
    return 0;
}
