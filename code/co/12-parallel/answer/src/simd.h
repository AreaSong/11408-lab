#ifndef PARALLEL_SIMD_H
#define PARALLEL_SIMD_H

#include <stdio.h>

/* lanes = 向量位数 / 元素位数。向量指令条数是 ceil(元素个数 / 通道数)。 */
typedef struct {
    int ok;
    int lanes;
    int vector_insns;
    long long speed_num;
    long long speed_den;
} SimdInfo;

SimdInfo simd_of(int vector_bits, int elem_bits, int count);
void simd_show(FILE *fp, int vector_bits, int elem_bits, int count);

#endif
