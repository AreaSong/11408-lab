#ifndef NET_FWD_H
#define NET_FWD_H

#include <stdint.h>

/* 转发表只留前缀、下一跳和出接口。hop 为 0 表示直连。 */
#define NET_FWD_MAX 12

typedef struct {
    uint32_t net;
    int prefix;
    uint32_t hop;
    char ifname[8];
} NetRoute;

typedef struct {
    NetRoute row[NET_FWD_MAX];
    int n;
} NetFib;

void net_fib_init(NetFib *fib);
int net_fib_add(NetFib *fib, const char *cidr, const char *hop, const char *ifname);
int net_fib_load(NetFib *fib, const char *id);
int net_fib_find(const NetFib *fib, uint32_t dst, char *hop, int hcap, char *ifname, int icap);

int net_fwd_diff(const unsigned char before[20], const unsigned char after[20], char *out, int cap);
int net_fwd_2011(char *src, int sc, int *ttl, unsigned *cksum, char *changed, int ccap);
int net_fwd_hop(unsigned char hdr[20], int *icmp_type, int *icmp_code);
int net_fwd_line(const char *id, char *out, int cap);
int net_route_line(const char *id, const char *dst, char *out, int cap);

#endif
