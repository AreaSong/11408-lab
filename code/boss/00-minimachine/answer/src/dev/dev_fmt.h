#ifndef DEV_FMT_H
#define DEV_FMT_H

#include <stddef.h>

/* 带宽和传输率里的 k/M/G 是 10^3/10^6/10^9。内部工具，不是题面。 */
int dev_parse_hz(double value, const char *unit, double *hz);
int dev_parse_rate(double value, const char *unit, double *bps);
int dev_parse_seconds(double value, const char *unit, double *seconds);
void dev_fmt_num(char *buf, size_t n, double x);
void dev_fmt_rate(char *buf, size_t n, double bytes_per_s);
void dev_fmt_time(char *buf, size_t n, double seconds);
void dev_fmt_pct(char *buf, size_t n, double ratio);

#endif
