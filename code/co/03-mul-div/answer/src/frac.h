#ifndef FRAC_H
#define FRAC_H

#include <stdbool.h>
#include <stddef.h>

/* 定点小数：符号单独放，mag 是小数点后的 n 位数值。真值 = ±mag / 2^n。 */

#define FRAC_MAX 8

typedef struct {
    int n;
    int sign;
    unsigned mag;
} Frac;

void frac_err(char *err, size_t n, const char *msg);
bool frac_same(Frac x, Frac y, char *err, size_t errn);
bool frac_parse(const char *text, Frac *out, char *err, size_t errn);

/* 负零的真值是 0。分子的分母是 2^n。 */
long long frac_num(Frac f);

void frac_plain(char *dst, size_t n, unsigned bits, int width);
void frac_dot(char *dst, size_t n, unsigned bits, int left, int mag);
void frac_text(char *dst, size_t n, Frac f);
void frac_ratio(char *dst, size_t n, long long num, int den_bits);

#endif
