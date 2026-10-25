#include "fp.h"

#include <stdio.h>
#include <string.h>

static void note(AddInfo *o, const char *text) {
    snprintf(o->note, sizeof o->note, "%s", text);
}

static void match_c(AddInfo *o) {
    uint32_t hardware = c_fadd(o->x_bits, o->y_bits);
    int z_nan = fp_view(o->z_bits, &FMT32).kind == KIND_NAN;
    int c_nan = fp_view(hardware, &FMT32).kind == KIND_NAN;
    o->z = fp_view(o->z_bits, &FMT32);
    if (z_nan || c_nan) {
        o->c_match = (z_nan && c_nan) ? 2 : 0;
        return;
    }
    o->c_match = (o->z_bits == hardware) ? 1 : 0;
}

static int special_case(AddInfo *o) {
    int x_nan = o->x.kind == KIND_NAN;
    int y_nan = o->y.kind == KIND_NAN;
    int x_inf = o->x.kind == KIND_INF;
    int y_inf = o->y.kind == KIND_INF;
    int x_zero = o->x.kind == KIND_ZERO;
    int y_zero = o->y.kind == KIND_ZERO;
    o->special = 1;
    if (x_nan || y_nan) {
        o->z_bits = x_nan ? o->x_bits : o->y_bits;
        note(o, "有 NaN。阶码全 1 且尾数不是 0 就是 NaN，和也是 NaN");
        return 1;
    }
    if (x_inf && y_inf && o->x.sign != o->y.sign) {
        o->z_bits = 0x7FC00000u;
        note(o, "无穷大加上相反符号的无穷大，结果是 NaN");
        return 1;
    }
    if (x_inf || y_inf) {
        o->z_bits = x_inf ? o->x_bits : o->y_bits;
        note(o, "一个操作数是无穷大，另一个是有限数，和就是这个无穷大");
        return 1;
    }
    if (x_zero && y_zero) {
        o->z_bits = (o->x.sign && o->y.sign) ? 0x80000000u : 0u;
        note(o, "两个零相加。只有两个都是 -0 时结果才是 -0，否则是 +0");
        return 1;
    }
    if (x_zero || y_zero) {
        o->z_bits = x_zero ? o->y_bits : o->x_bits;
        note(o, "一个操作数是 0，和等于另一个数");
        return 1;
    }
    o->special = 0;
    return 0;
}

static uint64_t shift_down(uint64_t value, int amount, int *lost) {
    if (amount <= 0) {
        *lost = 0;
        return value;
    }
    if (amount >= 64) {
        *lost = value != 0;
        return 0;
    }
    *lost = (value & ((1ULL << amount) - 1ULL)) != 0;
    return value >> amount;
}

static uint32_t normalize(uint64_t sum, int exp, int sign, int lost, AddInfo *o) {
    uint32_t sig24;
    int guard;
    int sticky;
    int lsb;
    int up;
    o->norm = 0;
    if ((sum & (1ULL << 56)) != 0) {
        lost |= (int)(sum & 1ULL);
        sum >>= 1;
        exp++;
        o->norm = 1;
    }
    while (exp > -126 && sum != 0 && (sum & (1ULL << 55)) == 0) {
        sum <<= 1;
        exp--;
        o->norm--;
    }
    sig24 = (uint32_t)((sum >> 32) & 0xFFFFFFu);
    guard = (int)((sum >> 31) & 1ULL);
    sticky = ((sum & 0x7FFFFFFFULL) != 0) || lost;
    lsb = (int)(sig24 & 1u);
    up = guard && (sticky || lsb);
    if (up) {
        sig24++;
        if (sig24 == (1u << 24)) {
            sig24 >>= 1;
            exp++;
        }
    }
    o->guard = guard;
    o->sticky = sticky;
    o->lsb = lsb;
    o->round_up = up;
    if (sig24 == 0) {
        o->over = (guard || sticky) ? -1 : 0;
        return (uint32_t)sign << 31;
    }
    if ((sig24 & (1u << 23)) == 0) {
        o->over = 0;
        return ((uint32_t)sign << 31) | sig24;
    }
    if (exp > 127) {
        o->over = 1;
        return ((uint32_t)sign << 31) | 0x7F800000u;
    }
    o->over = 0;
    return ((uint32_t)sign << 31) | ((uint32_t)(exp + 127) << 23) | (sig24 & 0x7FFFFFu);
}

static void load_finite(const View *v, int *exp, uint32_t *sig, int *sign) {
    *exp = v->exp_true;
    *sig = (uint32_t)v->sig;
    *sign = v->sign;
}

static void arith(AddInfo *o) {
    int exp_x, exp_y, sign_x, sign_y, diff, lost, y_small;
    uint32_t sig_x, sig_y, show_x, show_y;
    uint64_t ax, ay, sum;
    load_finite(&o->x, &exp_x, &sig_x, &sign_x);
    load_finite(&o->y, &exp_y, &sig_y, &sign_y);
    show_x = sig_x;
    show_y = sig_y;
    y_small = 1;
    if (exp_y > exp_x || (exp_y == exp_x && sig_y > sig_x)) {
        int tmp_e = exp_x;
        uint32_t tmp_s = sig_x;
        int tmp_g = sign_x;
        exp_x = exp_y;
        sig_x = sig_y;
        sign_x = sign_y;
        exp_y = tmp_e;
        sig_y = tmp_s;
        sign_y = tmp_g;
        y_small = 0;
    }
    diff = exp_x - exp_y;
    ax = (uint64_t)sig_x << 32;
    ay = shift_down((uint64_t)sig_y << 32, diff, &lost);
    if (sign_x == sign_y) {
        sum = ax + ay;
    } else if (lost) {
        /* 右移丢掉的位使小阶数被截成更小。减法要先减掉这不足 1 的一截，否则借位会少一次。 */
        sum = ax - ay - 1ULL;
    } else {
        sum = ax - ay;
        if (sum == 0) {
            sign_x = 0;
        }
    }
    o->x_sig = show_x;
    o->y_sig = show_y;
    o->diff = diff;
    o->y_is_small = y_small;
    o->aligned = (uint32_t)(ay >> 32);
    o->align_lost = lost;
    o->sum_sig = (uint32_t)((sum >> 32) & 0x1FFFFFFu);
    o->z_bits = normalize(sum, exp_x, sign_x, lost, o);
}

int fadd_info(uint32_t x, uint32_t y, AddInfo *out) {
    memset(out, 0, sizeof *out);
    out->x_bits = x;
    out->y_bits = y;
    out->x = fp_view(x, &FMT32);
    out->y = fp_view(y, &FMT32);
    if (!special_case(out)) {
        arith(out);
    }
    match_c(out);
    return 1;
}
