#ifndef PARALLEL_VEC_H
#define PARALLEL_VEC_H

/* 步骤 ④。n 必须是 4 的倍数。标量循环禁止自动向量化，NEON 显式一次处理 4 个 float。 */
void vec_scalar(const float *a, float *c, int n, int rounds);
void vec_neon(const float *a, float *c, int n, int rounds);

#endif
