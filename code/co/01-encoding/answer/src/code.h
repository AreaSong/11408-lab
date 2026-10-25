#ifndef CODE_H
#define CODE_H

#include "bits.h"
#include "number.h"

#include <stdio.h>

/* 考纲里的四种定点编码，再加无符号。移码 = 补码符号位取反。 */

typedef enum {
    KIND_UNSIGNED = 0,
    KIND_ORIG,
    KIND_ONES,
    KIND_COMP,
    KIND_BIAS
} CodeKind;

bool code_kind_parse(const char *text, CodeKind *out);
const char *code_kind_name(CodeKind kind);

bool code_width_ok(int width);
bool code_in_comp(int width, long long value);
long long code_comp_min(int width);
long long code_comp_max(int width);

/* 原码、反码表示不了 -2^(n-1) 时返回 false。0 编出来的是 +0。 */
bool code_encode(int width, long long value, CodeKind kind, Bits *out);
bool code_decode(Bits b, CodeKind kind, long long *value, bool *minus_zero);

/* 定点小数的真值 = units / 2^(width-1)。+1 放不进任何一种编码。 */
bool code_frac_units(int width, const Decimal *d, long long *units, char *err, size_t errn);

/* sign_fill 为真时复制最高位（补码、反码、有符号扩展）。原码扩展走 code_extend_orig。 */
Bits code_extend(Bits src, int to_width, bool sign_fill);
Bits code_extend_orig(Bits src, int to_width);
Bits code_narrow(Bits src, int to_width);

/* 低 width 位的和。*carry 是丢掉的最高位进位，这一关先不叫它 CF。 */
unsigned long long code_add(int width, unsigned long long a, unsigned long long b, int *carry);

/*
 * C 的整型转换：先按源类型的符号性把位补齐或截断，再按目标类型读。
 * 目标是无符号时 *as_unsigned 为真，*out_u 是那一串位的无符号值。
 */
bool code_retype(bool src_signed, bool dst_signed, int src_w, int dst_w, long long value,
                 Bits *stored, long long *out_s, unsigned long long *out_u, bool *as_unsigned,
                 char *err, size_t errn);

int code_print_int(FILE *out, int width, const char *text);
int code_print_frac(FILE *out, int width, const char *text);
int code_print_decode(FILE *out, bool frac, int width, CodeKind kind, const char *pattern);
int code_print_range(FILE *out, bool frac, int width);
int code_print_retype(FILE *out, bool src_signed, bool dst_signed, int src_w, int dst_w,
                      const char *text);
int code_print_orig_extend(FILE *out, int from_w, int to_w, const char *pattern);
int code_print_sum(FILE *out, int width, const char *a, const char *b);
int code_print_cmp(FILE *out, int width, const char *x, const char *y, const char *z);

#endif
