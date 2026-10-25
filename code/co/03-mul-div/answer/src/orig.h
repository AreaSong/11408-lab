#ifndef ORIG_H
#define ORIG_H

#include "frac.h"

#include <stdio.h>

/* 原码一位乘的一步：先按乘数末位决定加不加，再和乘数一起逻辑右移。 */

typedef struct {
    int add;
    unsigned part;
    unsigned mult;
    unsigned part_s;
    unsigned mult_s;
} OrigStep;

typedef struct {
    Frac x;
    Frac y;
    int sign;
    unsigned mag;
    int adds;
    OrigStep step[FRAC_MAX];
} Orig;

bool orig_eval(Frac x, Frac y, Orig *o, char *err, size_t errn);
int orig_print(FILE *out, const Orig *o);

#endif
