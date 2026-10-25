#include "orig.h"

/*
 * 原码一位乘：符号异或，数值位当无符号数逐位相加。
 * 部分积多留两位符号，是为了接住 |x| 加进来时的进位，右移仍是逻辑右移。
 */

bool orig_eval(Frac x, Frac y, Orig *o, char *err, size_t errn) {
    int n;
    int i;
    unsigned mask;
    unsigned a = 0;
    unsigned q;
    if (!frac_same(x, y, err, errn)) {
        return false;
    }
    n = x.n;
    mask = (1u << (n + 2)) - 1u;
    q = y.mag;
    o->x = x;
    o->y = y;
    o->sign = x.sign ^ y.sign;
    o->adds = 0;
    for (i = 0; i < n; i++) {
        OrigStep *s = &o->step[i];
        s->mult = q;
        s->add = (int)(q & 1u);
        if (s->add) {
            a = (a + x.mag) & mask;
            o->adds++;
        }
        s->part = a;
        s->part_s = a >> 1;
        s->mult_s = (q >> 1) | ((a & 1u) << (n - 1));
        a = s->part_s;
        q = s->mult_s;
    }
    o->mag = ((a & ((1u << n) - 1u)) << n) | q;
    return true;
}
