#ifndef UTIL_H
#define UTIL_H

/* 输入拆词、名字检查、最简分数。各模拟器共用，避免各写一份。 */
int util_split(const char *line, char out[][32], int max);
int util_ll(const char *text, long long *out);
int util_name_ok(const char *text);
long long util_gcd(long long a, long long b);
void util_print_frac(long long n, long long d);

#endif
