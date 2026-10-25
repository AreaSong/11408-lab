/*
 * IEEE 754 二进制浮点。单精度偏置 127，双精度偏置 1023。
 * 阶码用移码，但全 0、全 1 让给 0、非规格化、无穷和 NaN，所以偏置是 127 不是 128。
 */
#ifndef FP_H
#define FP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int sign;
    uint64_t coef;
    int exp10;
} Decimal;

typedef struct {
    int frac_bits;
    int exp_bits;
    int bias;
} Fmt;

extern const Fmt FMT32;
extern const Fmt FMT64;

typedef enum { KIND_ZERO, KIND_SUB, KIND_NORM, KIND_INF, KIND_NAN } Kind;

typedef struct {
    int sign;
    int exp_field;
    uint64_t frac;
    Kind kind;
    int exp_true;
    uint64_t sig;
} View;

typedef struct {
    uint64_t bits;
    int sign;
    int msb_exp;
    int guard;
    int sticky;
    int lsb;
    int rounded_up;
    int inexact;
    int is_zero;
    char plain[80];
    char sci[112];
} EncShow;

typedef struct {
    int special;
    char note[192];
    uint32_t x_bits;
    uint32_t y_bits;
    uint32_t z_bits;
    View x;
    View y;
    View z;
    int diff;
    int y_is_small;
    uint32_t x_sig;
    uint32_t y_sig;
    uint32_t aligned;
    uint32_t sum_sig;
    int align_lost;
    int norm;
    int guard;
    int sticky;
    int lsb;
    int round_up;
    int over;
    int c_match;
} AddInfo;

int dec_parse(const char *text, Decimal *out, char *err, size_t n);
int hex_parse(const char *text, int digits, uint64_t *out, char *err, size_t n);
int fp_encode(const Decimal *num, const Fmt *fmt, EncShow *out, char *err, size_t n);
View fp_view(uint64_t bits, const Fmt *fmt);
int fp_exam(const View *v, const Fmt *fmt, char *buf, size_t n);
int fp_decimal(const View *v, const Fmt *fmt, char *buf, size_t n);
void fp_hex(uint64_t bits, int digits, char *buf);
const char *fp_kind_name(Kind kind);

uint32_t c_float_bits(const char *text);
uint64_t c_double_bits(const char *text);
uint32_t c_fadd(uint32_t a, uint32_t b);
int fadd_info(uint32_t x, uint32_t y, AddInfo *out);

#endif
