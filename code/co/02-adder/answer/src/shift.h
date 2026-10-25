#ifndef SHIFT_H
#define SHIFT_H

#include "word.h"

#include <stdio.h>

/* 逻辑移位空位补 0。补码算术右移补符号位，算术左移仍补 0，但要看符号变不变。 */

typedef struct {
    Word result;
    int overflow;
} ShiftResult;

Word shift_once(Word w, int arith, int right, int *sign_changed);
ShiftResult shift_apply(Word w, int arith, int right, int count);
int shift_print(FILE *out, Word w, int arith, int right, int count);

#endif
