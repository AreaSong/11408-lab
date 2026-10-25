#ifndef BOOTH_H
#define BOOTH_H

#include "frac.h"

#include <stdio.h>

/*
 * 补码一位乘（Booth）。n 是数值位数，不含符号。
 * 共 n+1 次判断，只右移 n 次，最后一次不移位。
 */

typedef struct {
    int op;
    unsigned part;
    unsigned q;
    int extra;
    int shifted;
    unsigned part_s;
    unsigned q_s;
    int extra_s;
} BoothStep;

typedef struct {
    Frac x;
    Frac y;
    unsigned x_tc;
    unsigned nx_tc;
    unsigned y_tc;
    int signs_ok;
    unsigned product;
    long long value;
    int real_adds;
    BoothStep step[FRAC_MAX + 1];
} Booth;

bool booth_eval(Frac x, Frac y, Booth *o, char *err, size_t errn);
int booth_print(FILE *out, const Booth *o);

#endif
