#include "divide.h"

/*
 * 原码加减交替：符号异或。数值部分先试减，余数为负说明商还是小数。
 * 之后每步先记住符号再左移：正就减 |y|、商 1，负就加 |y|、商 0。
 * 最后一次若余数仍为负，要加回 |y|。中间那些负余数不必加回。
 */

static void divide_steps(Divide *o, int n, unsigned *r) {
    unsigned wmask = (1u << (n + 1)) - 1u;
    int i;
    o->q = 0;
    for (i = 0; i < n; i++) {
        DivStep *s = &o->step[i];
        unsigned shifted = (*r << 1) & wmask;
        s->prev_neg = (int)((*r >> n) & 1u);
        s->shifted = shifted;
        s->sign_flipped = (int)((shifted >> n) & 1u) != s->prev_neg;
        if (s->prev_neg) {
            *r = (shifted + o->y.mag) & wmask;
            s->add = 1;
        } else {
            *r = (shifted + o->neg_y) & wmask;
            s->add = 0;
        }
        s->rem = *r;
        s->qbit = ((s->rem >> n) & 1u) ? 0 : 1;
        o->q = (o->q << 1) | (unsigned)s->qbit;
    }
    o->q &= (1u << n) - 1u;
}

bool divide_eval(Frac x, Frac y, Divide *o, char *err, size_t errn) {
    int n;
    unsigned wmask;
    unsigned r;
    if (!frac_same(x, y, err, errn)) {
        return false;
    }
    n = x.n;
    o->x = x;
    o->y = y;
    o->kind = 0;
    if (y.mag == 0) {
        o->kind = 2;
        return true;
    }
    if (x.mag >= y.mag) {
        o->kind = 1;
        return true;
    }
    wmask = (1u << (n + 1)) - 1u;
    o->neg_y = ((1u << (n + 1)) - y.mag) & wmask;
    r = (x.mag + o->neg_y) & wmask;
    o->trial = r;
    divide_steps(o, n, &r);
    o->rem_before_fix = r;
    o->corrected = (int)((r >> n) & 1u);
    if (o->corrected) {
        r = (r + y.mag) & wmask;
    }
    o->machine_rem = r;
    o->qsign = x.sign ^ y.sign;
    o->rem_num = (x.sign && r != 0) ? -(long long)r : (long long)r;
    return true;
}
