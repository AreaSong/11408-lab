#ifndef CLA_H
#define CLA_H

#include "word.h"

/*
 * 4 位先行进位。G 是产生，P 是传递。
 * c[0] 是最低进位，c[1]..c[4] 由 G、P 同时算出来，不再等前一位的全加器。
 */

typedef struct {
    int g[4];
    int p[4];
    int c[5];
    unsigned long long sum;
} Cla4;

Cla4 cla4(Word a, Word b, int cin);

#endif
