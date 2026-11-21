#ifndef NET_WIN_H
#define NET_WIN_H

/* 协议名：stop、gbn、sr、slide。slide 按 GBN 的 2^n-1 取序号位数。 */
int net_need(const char *proto, long long bits_lo, long long bits_hi, int ack_same, long long bps,
             long long prop_ns, int util_pct, int *window, int *bits);
int net_rate(const char *proto, int window, long long bits, long long ack_bits, long long bps,
             long long prop_ns, long long *out);
int net_limit_eq(const char *proto, int nbits, int *send_w, int *recv_w);
int net_room(int nbits, int send_w, int *recv_w);
int net_cmp_u(int nbits, char *dst, int cap);

#endif
