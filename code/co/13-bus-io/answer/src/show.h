#ifndef SHOW_H
#define SHOW_H

#include <stdbool.h>
#include <stddef.h>

/* 把差一点就是考场十进制的二进制误差收掉。很小的时间不能收成 0。 */
double snap(double x);
void fmt_num(char *buf, size_t n, double x);

/* 频率、时间、传输率。带宽和 I/O 速率里的 k/M/G 是 10^3/10^6/10^9。 */
bool parse_hz(double value, const char *unit, double *hz);
bool parse_seconds(double value, const char *unit, double *seconds);
bool parse_rate(double value, const char *unit, double *bytes_per_s);

void fmt_hz(char *buf, size_t n, double hz);
void fmt_seconds(char *buf, size_t n, double seconds);
void fmt_rate(char *buf, size_t n, double bytes_per_s);
void fmt_rate_short(char *buf, size_t n, double bytes_per_s);
void fmt_time_short(char *buf, size_t n, double seconds);
void fmt_percent(char *buf, size_t n, double ratio);

#endif
