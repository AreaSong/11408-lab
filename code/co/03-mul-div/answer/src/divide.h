#ifndef DIVIDE_H
#define DIVIDE_H

#include "frac.h"

#include <stdio.h>

/* 原码加减交替（不恢复余数）。kind：0 正常，1 商溢出，2 除数为 0。 */

typedef struct {
    int prev_neg;
    unsigned shifted;
    int add;
    unsigned rem;
    int qbit;
    int sign_flipped;
} DivStep;

typedef struct {
    Frac x;
    Frac y;
    int kind;
    unsigned neg_y;
    unsigned trial;
    int qsign;
    unsigned q;
    unsigned machine_rem;
    int corrected;
    unsigned rem_before_fix;
    long long rem_num;
    DivStep step[FRAC_MAX];
} Divide;

bool divide_eval(Frac x, Frac y, Divide *o, char *err, size_t errn);
int divide_print(FILE *out, const Divide *o);

#endif
