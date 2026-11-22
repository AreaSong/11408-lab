#ifndef VEC_H
#define VEC_H

#include <stdio.h>

/* 向量地址 = 基址 + 类型号 × 表项字节数。失败返回 1。 */
int vec_address(unsigned base, unsigned stride, unsigned type, unsigned *out);

/* req[i] 为 1 表示该源的请求标记已置位。优先级数字越小越先响应。没有请求返回 1。 */
int vec_pick(const int *req, const int *pri, int n, int *winner);

int cmd_vec(FILE *out, const char *args);

#endif
