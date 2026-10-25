/*
 * 十进制转二进制时，5 的幂会超出 64 位。这里用 8 个 32 位肢体拼一个 256 位无符号数。
 * 肢体 0 是最低位。考纲不考大整数，它只为手算 IEEE 754 的舍入服务。
 */
#ifndef WIDE_H
#define WIDE_H

#include <stdint.h>

#define WIDE_LIMBS 8
#define WIDE_BITS (WIDE_LIMBS * 32)

typedef struct {
    uint32_t limb[WIDE_LIMBS];
} Wide;

void wide_clear(Wide *w);
void wide_set_u64(Wide *w, uint64_t v);
int wide_is_zero(const Wide *w);
int wide_bitlen(const Wide *w);
int wide_bit(const Wide *w, int index);
/* 成功返回 1。结果放不进 256 位就返回 0，w 的内容不再使用。 */
int wide_shl(Wide *w, int n);
int wide_mul_u32(Wide *w, uint32_t m);
int wide_cmp(const Wide *a, const Wide *b);
void wide_sub(Wide *a, const Wide *b);
int wide_low_nonzero(const Wide *w, int nbits);
void wide_div(const Wide *n, const Wide *d, Wide *q, Wide *r);

#endif
