#ifndef UNIT_H
#define UNIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
 * 计网里的 k / M / G 是 10^3 / 10^6 / 10^9。
 * 带宽、波特率、比特率都先化成整数再算，真题数据都能落在整数上。
 */

bool parse_hz(double value, const char *unit, long long *hz);
bool parse_bps(double value, const char *unit, long long *bps);
bool parse_baud(double value, const char *unit, long long *baud);
bool parse_count(double value, long long *out);

void fmt_hz(char *buf, size_t n, long long hz);
void fmt_bps(char *buf, size_t n, long long bps);
void fmt_baud(char *buf, size_t n, long long baud);

void unit_print_scale_note(FILE *out);

int floor_log2_u(unsigned long long x);
bool mul_ll(long long a, long long b, long long *out);

#endif
