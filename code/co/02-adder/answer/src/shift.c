#include "shift.h"

Word shift_once(Word w, int arith, int right, int *sign_changed) {
    int sign = word_bit(w, w.width - 1);
    unsigned long long bits;
    *sign_changed = 0;
    if (!right) {
        int new_sign = word_bit(w, w.width - 2);
        /* 左移时旧符号位被顶出去，新符号位来自次高位。两者不同就是溢出。 */
        *sign_changed = sign != new_sign;
        return word_make(w.width, w.bits << 1);
    }
    bits = w.bits >> 1;
    if (arith && sign) {
        bits |= 1ULL << (w.width - 1);
    }
    return word_make(w.width, bits);
}

ShiftResult shift_apply(Word w, int arith, int right, int count) {
    ShiftResult out;
    int i;
    out.overflow = 0;
    for (i = 0; i < count; i++) {
        int changed = 0;
        w = shift_once(w, arith, right, &changed);
        if (arith && !right && changed) {
            out.overflow = 1;
        }
    }
    out.result = w;
    return out;
}
