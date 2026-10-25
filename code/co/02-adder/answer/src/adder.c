#include "adder.h"

FullAdd full_add(int a, int b, int cin) {
    FullAdd out;
    int x = a & 1;
    int y = b & 1;
    int z = cin & 1;
    int xy = x ^ y;
    /* 先把两个加数相加，再把低位进位加进来。两次异或就是本位和。 */
    out.sum = xy ^ z;
    /* 两个加数都是 1，或者低位有进位且这一位本身是 1，都会向高位进位。 */
    out.cout = (x & y) | (z & xy);
    return out;
}

Ripple ripple_add(Word a, Word b, int cin) {
    Ripple out;
    int carry = cin & 1;
    int i;
    out.width = a.width;
    out.sum = 0;
    for (i = 0; i < 34; i++) {
        out.carry[i] = 0;
    }
    out.carry[0] = carry;
    for (i = 0; i < a.width; i++) {
        FullAdd bit = full_add(word_bit(a, i), word_bit(b, i), carry);
        if (bit.sum) {
            out.sum |= 1ULL << i;
        }
        carry = bit.cout;
        out.carry[i + 1] = carry;
    }
    return out;
}
