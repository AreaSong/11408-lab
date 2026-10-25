#ifndef BITS_H
#define BITS_H

#include <stddef.h>
#include <stdbool.h>

/* 机器数：只使用低 width 位。卷面的 int 到 32 位，double 的机器数到 64 位。 */

typedef struct {
    int width;
    unsigned long long bits;
} Bits;

unsigned long long bits_mask(int width);
bool bits_width_ok(int width);
Bits bits_make(int width, unsigned long long raw);

/* 成功时写入 *out。二进制必须写满字长；带 H 或 0x 的按十六进制，数值要放得进 width 位。 */
bool bits_parse(const char *text, int width, Bits *out, char *err, size_t errn);

/* 地址和字节值一律按十六进制，可写 0x 前缀或 H 后缀。 */
bool bits_parse_hex(const char *text, unsigned long long *out, char *err, size_t errn);

void bits_write_bin(char *dst, size_t n, Bits b);
/* 定点小数：符号位后面加小数点，例如 1.0100000 */
void bits_write_point(char *dst, size_t n, Bits b);
/* 从低位起每 4 个十六进制数字空一格，末尾加 H */
void bits_write_hex(char *dst, size_t n, Bits b);
void bits_write_addr(char *dst, size_t n, unsigned long long addr);

#endif
