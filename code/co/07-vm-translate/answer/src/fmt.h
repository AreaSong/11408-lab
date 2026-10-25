#ifndef FMT_H
#define FMT_H

#include <stddef.h>
#include <stdint.h>

/* 十进制、0x 前缀，或末尾 H 的十六进制。拒绝负号。 */
int fmt_parse(const char *text, uint64_t *out);

/* bits > 0 时按字段宽度补前导 0，写成 0x 开头，避免末尾的 H 被当成一位十六进制数字。 */
int fmt_hex(char *dst, size_t n, uint64_t value, int bits);

/* 考卷写法：数字末尾加 H。H 是进制记号，不是一位数字。 */
int fmt_h(char *dst, size_t n, uint64_t value, int bits);

/* 固定位数的二进制，末尾加 B。 */
int fmt_bin(char *dst, size_t n, uint64_t value, int bits);

#endif
