#include "booth.h"

/* 负数的补码是 2^width - 数值。负零在补码里就是 0，没有单独的符号。 */

static unsigned tc_encode(int sign, unsigned mag, int width) {
    unsigned mask = (1u << width) - 1u;
    if (sign == 0 || mag == 0) {
        return mag & mask;
    }
    return ((1u << width) - mag) & mask;
}

static void booth_finish(Booth *o, int n, unsigned a, unsigned q) {
    unsigned sign = (a >> (n + 1)) & 1u;
    unsigned sign2 = (a >> n) & 1u;
    unsigned high = a & ((1u << n) - 1u);
    unsigned low = (q >> 1) & ((1u << n) - 1u);
    int width = 2 * n + 1;
    o->signs_ok = sign == sign2;
    o->product = (sign << (2 * n)) | (high << n) | low;
    if (sign) {
        o->value = (long long)o->product - (1LL << width);
    } else {
        o->value = (long long)o->product;
    }
}

static void booth_shift(BoothStep *s, int n, unsigned *a, unsigned *q, unsigned *extra) {
    unsigned a_lsb = *a & 1u;
    unsigned signbit = (*a >> (n + 1)) & 1u;
    unsigned qmask = (1u << (n + 1)) - 1u;
    *a = (*a >> 1) | (signbit << (n + 1));
    *extra = *q & 1u;
    *q = ((*q >> 1) | (a_lsb << n)) & qmask;
    s->shifted = 1;
    s->part_s = *a;
    s->q_s = *q;
    s->extra_s = (int)*extra;
}

bool booth_eval(Frac x, Frac y, Booth *o, char *err, size_t errn) {
    int n;
    int i;
    unsigned mask;
    unsigned a = 0;
    unsigned q;
    unsigned extra = 0;
    if (!frac_same(x, y, err, errn)) {
        return false;
    }
    n = x.n;
    mask = (1u << (n + 2)) - 1u;
    o->x = x;
    o->y = y;
    o->x_tc = tc_encode(x.sign, x.mag, n + 2);
    o->nx_tc = (~o->x_tc + 1u) & mask;
    o->y_tc = tc_encode(y.sign, y.mag, n + 1);
    o->real_adds = 0;
    q = o->y_tc;
    for (i = 0; i <= n; i++) {
        BoothStep *s = &o->step[i];
        unsigned low = q & 1u;
        s->q = q;
        s->extra = (int)extra;
        s->op = 0;
        s->shifted = 0;
        if (low == 0u && extra == 1u) {
            a = (a + o->x_tc) & mask;
            s->op = 1;
        } else if (low == 1u && extra == 0u) {
            a = (a + o->nx_tc) & mask;
            s->op = -1;
        }
        if (s->op != 0) {
            o->real_adds++;
        }
        s->part = a;
        if (i < n) {
            booth_shift(s, n, &a, &q, &extra);
        }
    }
    booth_finish(o, n, a, q);
    return true;
}
