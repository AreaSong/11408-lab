#ifndef NUM_H
#define NUM_H

#include <stdint.h>
#include <stdio.h>

/* 十进制、0x 前缀、末尾 H 都接受。负号作用在数值上，FFFFH 仍是正的位模式。 */
int num_parse(const char *text, int64_t *out);

int num_pow2(int bits, uint64_t *out);

/* 最小的 b，使 2^b >= n。n 为 0 时失败。 */
int num_ceil_log2(uint64_t n, int *bits);

/* 把低 width 位按补码读成真值。地址字段是无符号的，位移量才走这里。 */
int64_t num_sext(uint64_t bits, int width);

/* 大于 16 位的地址补到 8 个十六进制数字，和考卷上的 08048100H 对齐。 */
void num_print_addr(FILE *out, uint64_t value);

int num_split(char *line, char **tok, int max);

#endif
