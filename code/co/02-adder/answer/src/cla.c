#include "cla.h"

Cla4 cla4(Word a, Word b, int cin) {
    Cla4 out;
    int i;
    int c0 = cin & 1;
    for (i = 0; i < 4; i++) {
        int abit = word_bit(a, i);
        int bbit = word_bit(b, i);
        out.g[i] = abit & bbit;
        out.p[i] = abit ^ bbit;
    }
    out.c[0] = c0;
    /* 展开后每一级进位只依赖 G、P 和 c0，所以四级可以同时出来。 */
    out.c[1] = out.g[0] | (out.p[0] & c0);
    out.c[2] = out.g[1] | (out.p[1] & out.g[0]) | (out.p[1] & out.p[0] & c0);
    out.c[3] = out.g[2] | (out.p[2] & out.g[1]) | (out.p[2] & out.p[1] & out.g[0]) |
               (out.p[2] & out.p[1] & out.p[0] & c0);
    out.c[4] = out.g[3] | (out.p[3] & out.g[2]) | (out.p[3] & out.p[2] & out.g[1]) |
               (out.p[3] & out.p[2] & out.p[1] & out.g[0]) |
               (out.p[3] & out.p[2] & out.p[1] & out.p[0] & c0);
    out.sum = 0;
    for (i = 0; i < 4; i++) {
        if (out.p[i] ^ out.c[i]) {
            out.sum |= 1ULL << i;
        }
    }
    return out;
}
