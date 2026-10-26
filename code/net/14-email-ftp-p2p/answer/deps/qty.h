#ifndef QTY_H
#define QTY_H

#include <stdbool.h>
#include <stdio.h>

/*
 * 计网带宽、速率里的 k/M/G 是 10^3、10^6、10^9。
 * 时间内部用纳秒整数，避免 0.08 ms 这类十进制在 double 里印出长尾巴。
 * 真题数据换算后都能整除；不能整除就拒绝，不在这里四舍五入。
 */

bool parse_bits(double value, const char *unit, long long *bits);
bool parse_bps(double value, const char *unit, long long *bps);
bool parse_ns(double value, const char *unit, long long *ns);
bool parse_meters(double value, const char *unit, long long *meters);
bool parse_mps(double value, const char *unit, long long *mps);

/* out = a * b / den，den 必须整除乘积，并且乘积不溢出 */
bool qty_mul_div(long long a, long long b, long long den, long long *out);

/* 纳秒 = 比特数 × 10^9 / 每秒比特数。发送时延、传播时延都是这个除法 */
bool qty_div_ns(long long amount, long long per_sec, long long *ns);

void fmt_bits(char *buf, size_t n, long long bits);
void fmt_bps(char *buf, size_t n, long long bps);
void fmt_meters(char *buf, size_t n, long long meters);

/* 0.00008 s = 0.08 ms 这种对照；0 只写成 0 s。毫秒的小数至少两位 */
void fmt_time(char *buf, size_t n, long long ns);

/* 算式中间项只用一个单位：整秒写 s，否则写 ms */
void fmt_time_one(char *buf, size_t n, long long ns);

/* 比值两边用同一单位，避免 800 ms / 2 s 这种除法 */
void fmt_time_ms(char *buf, size_t n, long long ns);

/* 只写比特和 k/M/G，不换算成字节。时延带宽积的单位是比特 */
void fmt_span(char *buf, size_t n, long long bits);

void qty_print_scale_note(FILE *out);

#endif
