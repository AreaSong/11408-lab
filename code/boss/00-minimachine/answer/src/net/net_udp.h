#ifndef NET_UDP_H
#define NET_UDP_H

#include <stdint.h>

/* 反码求和后再取反。算出 0 时改记 0xffff，和「没算校验和」分开。 */
unsigned net_udp_field(unsigned folded);
unsigned net_udp_fold2(unsigned hi, unsigned lo);

int net_udp_cksum(uint32_t src, uint32_t dst, int sport, int dport, const unsigned char *data, int n,
                  unsigned *sum);
int net_udp_verify(uint32_t src, uint32_t dst, int sport, int dport, const unsigned char *data, int n);

/* 应用层字节数。UDP 首部 8，TCP 最少 20。返回值是百分数的十分位，60.0% 写成 600。 */
int net_udp_eff(int app, int *udp_tenths, int *tcp_tenths);
int net_udp_head(const char *hex, int *sport, int *dport, int *data, unsigned *sum);
int net_udp_line(const char *id, char *out, int cap);

#endif
