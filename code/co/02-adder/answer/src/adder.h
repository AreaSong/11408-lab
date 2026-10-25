#ifndef ADDER_H
#define ADDER_H

#include "word.h"

/* 一位全加器的两个输出。和与进位都只是 0 或 1。 */

typedef struct {
    int sum;
    int cout;
} FullAdd;

/* 只用与、或、异或，不用 C 的加法。C 的 + 会把进位藏起来。 */
FullAdd full_add(int a, int b, int cin);

/*
 * n 位串行进位：n 个全加器同时接上操作数，进位从低位逐级送到高位。
 * carry[0] 是最低进位，carry[i] 是送进位 i 的进位，carry[width] 是最高位送出去的进位。
 */
typedef struct {
    int width;
    unsigned long long sum;
    int carry[34];
} Ripple;

Ripple ripple_add(Word a, Word b, int cin);

#endif
