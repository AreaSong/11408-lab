#ifndef EXAM_H
#define EXAM_H

#include "word.h"

#include <stdio.h>

/* 2020 的 2n 位乘积，以及截断到 n 位之后溢不溢出。 */

typedef struct {
    unsigned long long uprod;
    unsigned long long sprod;
    int u_over;
    int s_over;
} Prod;

bool prod_eval(Word x, Word y, Prod *p);
int prod_print(FILE *out, Word x, Word y);
int impl_print(FILE *out, const char *which);
int cdiv_print(FILE *out, Word dividend, Word divisor);

unsigned long long cdiv_r(Word dividend);
unsigned long long cdiv_q(Word dividend);
unsigned long long cdiv_y(Word divisor);

#endif
