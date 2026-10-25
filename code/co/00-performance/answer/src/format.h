#ifndef FORMAT_H
#define FORMAT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * 把二进制浮点收成「考场上会写出的那个十进制」。
 * 性能题的输入都是有限小数，直接打印 double 会冒出 16.799999999。
 */
double snap(double x);

/* 把 snap 之后的数写成不带无意义尾零的十进制；很大或很小才用科学计数法 */
void fmt_num(char *buf, size_t n, double x);

/* 主频。性能里的 k/M/G/T 是 10^3、10^6、10^9、10^12，不是 2 的幂 */
bool parse_hz(double value, const char *unit, double *hz);
void fmt_hz(char *buf, size_t n, double hz);

/* 时间。us 表示微秒 μs，避免输入法里的 μ */
bool parse_seconds(double value, const char *unit, double *seconds);
void fmt_seconds(char *buf, size_t n, double seconds);

/*
 * 部件加速比。两种写法：
 *   1.5     直接就是 S
 *   +50%    速度提高 50%，S = 1.5，不是时间变成一半
 * is_percent 为真时 *percent 是 50 这样的百分数。
 */
bool parse_speedup(const char *text, double *speedup, bool *is_percent, double *percent);

/* MFLOPS … ZFLOPS，指数是 6,9,…,21 */
int flops_unit_count(void);
const char *flops_unit_name(int index);
int flops_unit_exp(int index);
bool parse_flops_unit(const char *unit, int *exp10);

#endif
