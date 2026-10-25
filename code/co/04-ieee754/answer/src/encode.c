#include "fp.h"
#include "wide.h"

#include <stdio.h>
#include <string.h>

/* 商要比尾数多留这么多位，舍入位才落在已知的比特里，而不是被余数糊成一个粘滞位。 */
#define EXTRA_BITS 120

const Fmt FMT32 = {23, 8, 127};
const Fmt FMT64 = {52, 11, 1023};

static int bit_at(const Wide *w, int lsb_exp, int weight) {
    int idx = weight - lsb_exp;
    if (idx < 0 || idx >= WIDE_BITS) {
        return 0;
    }
    return wide_bit(w, idx);
}

static int sticky_below(const Wide *w, int lsb_exp, int weight, int rem) {
    int idx = weight - lsb_exp;
    if (idx <= 0) {
        return rem;
    }
    if (idx >= WIDE_BITS) {
        return rem || !wide_is_zero(w);
    }
    return rem || wide_low_nonzero(w, idx);
}

static int fail(char *err, size_t n, const char *msg) {
    snprintf(err, n, "%s", msg);
    return 0;
}

static int mul5(Wide *w, int times, char *err, size_t n) {
    int i;
    for (i = 0; i < times; i++) {
        if (!wide_mul_u32(w, 5)) {
            return fail(err, n, "这个数太大，手算用的 256 位不够");
        }
    }
    return 1;
}

static int coef_bits(uint64_t coef) {
    int n = 0;
    while (coef != 0) {
        coef >>= 1;
        n++;
    }
    return n == 0 ? 1 : n;
}

static int build_mag(const Decimal *num, Wide *mag, int *lsb_exp, int *rem, char *err, size_t n) {
    if (num->exp10 >= 0) {
        wide_set_u64(mag, num->coef);
        if (!mul5(mag, num->exp10, err, n)) {
            return 0;
        }
        *lsb_exp = num->exp10;
        *rem = 0;
        return 1;
    }
    {
        int q = -num->exp10;
        int need;
        Wide divisor;
        Wide quot;
        Wide residue;
        wide_set_u64(&divisor, 1);
        if (!mul5(&divisor, q, err, n)) {
            return 0;
        }
        need = wide_bitlen(&divisor) + EXTRA_BITS - coef_bits(num->coef);
        if (need < 0) {
            need = 0;
        }
        wide_set_u64(mag, num->coef);
        if (!wide_shl(mag, need)) {
            return fail(err, n, "这个数太小，手算用的 256 位不够");
        }
        wide_div(mag, &divisor, &quot, &residue);
        *mag = quot;
        *lsb_exp = -q - need;
        *rem = !wide_is_zero(&residue);
        return 1;
    }
}

typedef struct {
    uint64_t sig;
    int width;
    int exp_true;
    int sub;
    int guard;
    int sticky;
    int msb;
} Cut;

static int extract(const Wide *mag, int lsb_exp, int rem, const Fmt *fmt, Cut *c, char *err, size_t n) {
    int emin = 1 - fmt->bias;
    int fb = fmt->frac_bits;
    int bl = wide_bitlen(mag);
    int top;
    int round_w;
    int i;
    c->msb = lsb_exp + bl - 1;
    if (c->msb >= emin) {
        c->sub = 0;
        c->exp_true = c->msb;
        c->width = fb + 1;
        top = c->msb;
    } else {
        c->sub = 1;
        c->exp_true = emin;
        c->width = fb;
        top = emin - 1;
    }
    round_w = top - c->width;
    if (round_w < lsb_exp && rem) {
        return fail(err, n, "内部精度不够，换一个短一点的数");
    }
    c->sig = 0;
    for (i = 0; i < c->width; i++) {
        c->sig = (c->sig << 1) | (uint64_t)bit_at(mag, lsb_exp, top - i);
    }
    c->guard = bit_at(mag, lsb_exp, round_w);
    c->sticky = sticky_below(mag, lsb_exp, round_w, rem);
    return 1;
}

static int round_cut(Cut *c) {
    int lsb = (int)(c->sig & 1ULL);
    int up = c->guard && (c->sticky || lsb);
    if (!up) {
        return lsb;
    }
    c->sig += 1;
    if ((c->sig >> c->width) == 0) {
        return lsb;
    }
    if (c->sub) {
        c->sub = 0;
        c->sig = 1ULL << c->width;
        return lsb;
    }
    c->sig >>= 1;
    c->exp_true += 1;
    return lsb;
}

static uint64_t pack_cut(int sign, const Cut *c, const Fmt *fmt) {
    int fb = fmt->frac_bits;
    int emax_field = (1 << fmt->exp_bits) - 1;
    int exp_field;
    uint64_t frac;
    uint64_t bits;
    if (!c->sub && c->exp_true > fmt->bias) {
        exp_field = emax_field;
        frac = 0;
    } else if (c->sub) {
        exp_field = 0;
        frac = c->sig;
    } else if (c->sig == 0) {
        exp_field = 0;
        frac = 0;
    } else {
        exp_field = c->exp_true + fmt->bias;
        frac = c->sig & ((1ULL << fb) - 1ULL);
    }
    bits = ((uint64_t)exp_field << fb) | frac;
    if (sign) {
        bits |= 1ULL << (fmt->exp_bits + fb);
    }
    return bits;
}

static void append_pow(char *buf, size_t n, const char *body, int exp) {
    if (exp < 0) {
        snprintf(buf, n, "%s × 2^(%d)", body, exp);
    } else {
        snprintf(buf, n, "%s × 2^%d", body, exp);
    }
}

/* 竖线右边才是舍入时真正丢掉的位。规格化数看 1.M，非规格化数先对齐到 2^emin 的 0.M。 */
static void build_sci(const Wide *mag, int lsb_exp, int rem, const Cut *c, const Fmt *fmt, char *buf, size_t n) {
    int fb = fmt->frac_bits;
    int emin = 1 - fmt->bias;
    int top = c->sub ? (emin - 1) : c->msb;
    int round_w = top - c->width;
    char body[96];
    int p = 0;
    int i;
    if (c->sub) {
        body[p++] = '0';
        body[p++] = '.';
        for (i = 0; i < fb; i++) {
            body[p++] = bit_at(mag, lsb_exp, top - i) ? '1' : '0';
        }
    } else {
        body[p++] = '1';
        body[p++] = '.';
        for (i = 1; i <= fb; i++) {
            body[p++] = bit_at(mag, lsb_exp, c->msb - i) ? '1' : '0';
        }
    }
    body[p++] = '|';
    body[p++] = bit_at(mag, lsb_exp, round_w) ? '1' : '0';
    if (sticky_below(mag, lsb_exp, round_w, rem)) {
        body[p++] = '.';
        body[p++] = '.';
        body[p++] = '.';
    }
    body[p] = '\0';
    append_pow(buf, n, body, c->sub ? emin : c->msb);
}

static void build_plain(const Wide *mag, int lsb_exp, int rem, char *buf, size_t n) {
    int bl;
    int hi;
    int lo;
    int from;
    int to;
    int w;
    int p = 0;
    char tmp[80];
    buf[0] = '\0';
    if (rem || wide_is_zero(mag)) {
        return;
    }
    bl = wide_bitlen(mag);
    hi = lsb_exp + bl - 1;
    lo = hi;
    for (w = 0; w < bl; w++) {
        if (wide_bit(mag, w)) {
            lo = lsb_exp + w;
            break;
        }
    }
    from = hi > 0 ? hi : 0;
    to = lo < 0 ? lo : 0;
    if (from - to > 48) {
        return;
    }
    for (w = from; w >= to; w--) {
        if (w == -1) {
            tmp[p++] = '.';
        }
        tmp[p++] = bit_at(mag, lsb_exp, w) ? '1' : '0';
    }
    if (to == 0) {
        tmp[p++] = '.';
        tmp[p++] = '0';
    }
    tmp[p++] = 'B';
    tmp[p] = '\0';
    snprintf(buf, n, "%s", tmp);
}

static void zero_show(int sign, const Fmt *fmt, EncShow *out) {
    memset(out, 0, sizeof *out);
    out->sign = sign;
    out->is_zero = 1;
    if (sign) {
        out->bits = 1ULL << (fmt->exp_bits + fmt->frac_bits);
    }
    snprintf(out->plain, sizeof out->plain, "0");
    snprintf(out->sci, sizeof out->sci, "0");
}

int fp_encode(const Decimal *num, const Fmt *fmt, EncShow *out, char *err, size_t n) {
    Wide mag;
    Cut cut;
    char out_sci[112];
    char out_plain[80];
    int lsb_exp = 0;
    int rem = 0;
    int lsb;
    if (num->coef == 0) {
        zero_show(num->sign, fmt, out);
        return 1;
    }
    if (!build_mag(num, &mag, &lsb_exp, &rem, err, n)) {
        return 0;
    }
    if (wide_is_zero(&mag)) {
        if (!rem) {
            zero_show(num->sign, fmt, out);
            return 1;
        }
        return fail(err, n, "内部精度不够，换一个短一点的数");
    }
    if (!extract(&mag, lsb_exp, rem, fmt, &cut, err, n)) {
        return 0;
    }
    build_sci(&mag, lsb_exp, rem, &cut, fmt, out_sci, sizeof out_sci);
    build_plain(&mag, lsb_exp, rem, out_plain, sizeof out_plain);
    lsb = round_cut(&cut);
    memset(out, 0, sizeof *out);
    out->sign = num->sign;
    out->msb_exp = cut.msb;
    out->guard = cut.guard;
    out->sticky = cut.sticky;
    out->lsb = lsb;
    out->rounded_up = cut.guard && (cut.sticky || lsb);
    out->inexact = cut.guard || cut.sticky;
    out->bits = pack_cut(num->sign, &cut, fmt);
    snprintf(out->sci, sizeof out->sci, "%s", out_sci);
    snprintf(out->plain, sizeof out->plain, "%s", out_plain);
    return 1;
}
