#ifndef UNITS_H
#define UNITS_H

#include <stddef.h>

/* 长度：纯数字是字节，KB/K = 1024 字节（2016-41 写明 K=1024）。时间必须带 s 或 ms。 */
int parse_bytes(const char *s, long *out);
int parse_time_us(const char *s, long *out);
int parse_count(const char *s, int *out);

void fmt_bytes(long n, char *out, size_t cap);
void fmt_time_us(long us, char *out, size_t cap);
/* 超时重传的样本用秒，0.75 s 不要收成 750 ms。 */
void fmt_seconds(long us, char *out, size_t cap);

#endif
