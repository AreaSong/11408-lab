#ifndef WORD_H
#define WORD_H

#include <stddef.h>
#include <stdbool.h>

/*
 * 定长机器数，只用低 width 位。卷面的 int 到 32 位。
 * 位 0 是最低位。补码把最高位的权当成 -2^(width-1)。
 */

typedef struct {
    int width;
    unsigned long long bits;
} Word;

bool word_width_ok(int width);
unsigned long long word_mask(int width);
int word_bit(Word w, int index);
Word word_make(int width, unsigned long long raw);
long long word_smin(int width);
long long word_smax(int width);
long long word_signed(Word w);
unsigned long long word_unsigned(Word w);

/*
 * 机器数：0b 后跟刚好 width 位的 0/1，或带 H / 0x 的十六进制。
 * 真值：有符号十进制（可写负号，或在末尾加 s）；无符号十进制末尾加 u。
 * 全是 0 和 1、长度又等于位宽的十进制，必须加 s 或 u，避免和二进制撞车。
 */
bool word_parse(const char *text, int width, Word *out, char *err, size_t errn);

void word_write_bin(char *dst, size_t n, Word w);
void word_write_hex(char *dst, size_t n, Word w);

#endif
