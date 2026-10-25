#include "wide.h"

void wide_clear(Wide *w) {
    int i;
    for (i = 0; i < WIDE_LIMBS; i++) {
        w->limb[i] = 0;
    }
}

void wide_set_u64(Wide *w, uint64_t v) {
    wide_clear(w);
    w->limb[0] = (uint32_t)v;
    w->limb[1] = (uint32_t)(v >> 32);
}

int wide_is_zero(const Wide *w) {
    int i;
    for (i = 0; i < WIDE_LIMBS; i++) {
        if (w->limb[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static int limb_width(uint32_t limb) {
    int n = 0;
    while (limb != 0) {
        limb >>= 1;
        n++;
    }
    return n;
}

int wide_bitlen(const Wide *w) {
    int i;
    for (i = WIDE_LIMBS - 1; i >= 0; i--) {
        if (w->limb[i] != 0) {
            return i * 32 + limb_width(w->limb[i]);
        }
    }
    return 0;
}

int wide_bit(const Wide *w, int index) {
    if (index < 0 || index >= WIDE_BITS) {
        return 0;
    }
    return (int)((w->limb[index / 32] >> (index % 32)) & 1u);
}

int wide_low_nonzero(const Wide *w, int nbits) {
    int full;
    int rem;
    int i;
    if (nbits <= 0) {
        return 0;
    }
    if (nbits >= WIDE_BITS) {
        return !wide_is_zero(w);
    }
    full = nbits / 32;
    rem = nbits % 32;
    for (i = 0; i < full; i++) {
        if (w->limb[i] != 0) {
            return 1;
        }
    }
    if (rem != 0 && (w->limb[full] & ((1u << rem) - 1u)) != 0) {
        return 1;
    }
    return 0;
}

int wide_shl(Wide *w, int n) {
    int limbs;
    int bits;
    int i;
    uint32_t carry;
    if (n <= 0) {
        return 1;
    }
    if (n >= WIDE_BITS) {
        if (wide_is_zero(w)) {
            return 1;
        }
        return 0;
    }
    limbs = n / 32;
    bits = n % 32;
    if (limbs > 0) {
        for (i = WIDE_LIMBS - limbs; i < WIDE_LIMBS; i++) {
            if (w->limb[i] != 0) {
                return 0;
            }
        }
        for (i = WIDE_LIMBS - 1; i >= limbs; i--) {
            w->limb[i] = w->limb[i - limbs];
        }
        for (i = 0; i < limbs; i++) {
            w->limb[i] = 0;
        }
    }
    if (bits == 0) {
        return 1;
    }
    for (i = WIDE_LIMBS - 1; i >= 0; i--) {
        if ((w->limb[i] >> (32 - bits)) != 0 && i == WIDE_LIMBS - 1) {
            return 0;
        }
    }
    carry = 0;
    for (i = 0; i < WIDE_LIMBS; i++) {
        uint32_t next = w->limb[i] >> (32 - bits);
        w->limb[i] = (w->limb[i] << bits) | carry;
        carry = next;
    }
    return 1;
}

int wide_mul_u32(Wide *w, uint32_t m) {
    uint64_t carry = 0;
    int i;
    for (i = 0; i < WIDE_LIMBS; i++) {
        uint64_t prod = (uint64_t)w->limb[i] * m + carry;
        w->limb[i] = (uint32_t)prod;
        carry = prod >> 32;
    }
    return carry == 0;
}

int wide_cmp(const Wide *a, const Wide *b) {
    int i;
    for (i = WIDE_LIMBS - 1; i >= 0; i--) {
        if (a->limb[i] != b->limb[i]) {
            return a->limb[i] > b->limb[i] ? 1 : -1;
        }
    }
    return 0;
}

void wide_sub(Wide *a, const Wide *b) {
    uint64_t borrow = 0;
    int i;
    for (i = 0; i < WIDE_LIMBS; i++) {
        uint64_t av = a->limb[i];
        uint64_t bv = (uint64_t)b->limb[i] + borrow;
        if (av < bv) {
            a->limb[i] = (uint32_t)(av + (1ULL << 32) - bv);
            borrow = 1;
        } else {
            a->limb[i] = (uint32_t)(av - bv);
            borrow = 0;
        }
    }
}

static void wide_set_bit(Wide *w, int index) {
    w->limb[index / 32] |= 1u << (index % 32);
}

void wide_div(const Wide *n, const Wide *d, Wide *q, Wide *r) {
    int len = wide_bitlen(n);
    int i;
    wide_clear(q);
    wide_clear(r);
    for (i = len - 1; i >= 0; i--) {
        wide_shl(r, 1);
        if (wide_bit(n, i)) {
            r->limb[0] |= 1u;
        }
        if (wide_cmp(r, d) >= 0) {
            wide_sub(r, d);
            wide_set_bit(q, i);
        }
    }
}
