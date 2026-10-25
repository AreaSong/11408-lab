#include "exam.h"

static unsigned long long low_mask(int width) {
    if (width >= 64) {
        return ~0ULL;
    }
    return (1ULL << width) - 1ULL;
}

static unsigned long long wide_mask(int width) {
    if (width >= 32) {
        return ~0ULL;
    }
    return (1ULL << (2 * width)) - 1ULL;
}

bool prod_eval(Word x, Word y, Prod *p) {
    int width = x.width;
    unsigned long long xu = word_unsigned(x);
    unsigned long long yu = word_unsigned(y);
    long long xs = word_signed(x);
    long long ys = word_signed(y);
    unsigned long long low = low_mask(width);
    unsigned long long sbits;
    unsigned long long high;
    unsigned long long ext;
    unsigned long long sign;
    p->uprod = (xu * yu) & wide_mask(width);
    sbits = (unsigned long long)(xs * ys);
    p->sprod = sbits & wide_mask(width);
    p->u_over = (p->uprod >> width) != 0ULL;
    sign = (p->sprod >> (width - 1)) & 1ULL;
    high = p->sprod >> width;
    ext = sign ? low : 0ULL;
    p->s_over = high != ext;
    return true;
}

unsigned long long cdiv_r(Word dividend) {
    if (word_bit(dividend, dividend.width - 1)) {
        return 0xFFFFFFFFULL;
    }
    return 0;
}

unsigned long long cdiv_q(Word dividend) {
    return word_unsigned(dividend);
}

unsigned long long cdiv_y(Word divisor) {
    return word_unsigned(divisor);
}
