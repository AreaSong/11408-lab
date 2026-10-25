#ifndef UNIT_H
#define UNIT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* 把「差一点就是考场上那个十进制」的二进制误差收掉。很大或很小的数不硬收成 0。 */
double snap(double x);
void fmt_num(char *buf, size_t n, double x);

/* 存储容量。K/M/G/T = 2^10/2^20/2^30/2^40，B 和 KB 也一样。结果必须是整数。 */
bool parse_bin(double value, const char *unit, uint64_t *out);
/* 同一套字母按 10^3 计。磁盘题里两种口径都要能算出来。 */
bool parse_dec_bytes(double value, const char *unit, double *out);
/* 传输率。KB/s、MB/s、GB/s 的 K/M/G = 10^3/10^6/10^9。 */
bool parse_rate(double value, const char *unit, double *bytes_per_s);
/* 主频、总线频率。k/M/G = 10^3/10^6/10^9。 */
bool parse_hz(double value, const char *unit, double *hz);
/* 时间。us 就是微秒。 */
bool parse_seconds(double value, const char *unit, double *seconds);
/* 0B1FH、804001AH 或 0x804001A。地址按十六进制读，避免和十进制序号混在一起。 */
bool parse_addr(const char *text, uint64_t *addr);

int exact_log2_u64(uint64_t x);
int ceil_log2_u64(uint64_t x);

/* 64 us、4 M、0800H 这种考场写法 */
void fmt_time_unit(char *buf, size_t n, double seconds, const char *unit);
void fmt_bin_count(char *buf, size_t n, uint64_t count);
void fmt_hex(char *buf, size_t n, uint64_t value);
void fmt_hex_pad(char *buf, size_t n, uint64_t value, int digits);
void fmt_bits(char *buf, size_t n, uint64_t value, int bits);

#endif
