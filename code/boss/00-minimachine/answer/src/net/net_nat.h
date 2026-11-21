#ifndef NET_NAT_H
#define NET_NAT_H

#include <stdint.h>

/* 键是地址加端口。公网侧的一对不能重复，回来的分组才知道交给谁。 */
#define NET_NAT_MAX 8

typedef struct {
    uint32_t in_ip;
    int in_port;
    uint32_t out_ip;
    int out_port;
    int used;
} NetNatRow;

typedef struct {
    NetNatRow row[NET_NAT_MAX];
    int n;
    uint32_t pub;
} NetNat;

void net_nat_init(NetNat *box, uint32_t pub);
int net_nat_map(NetNat *box, uint32_t in_ip, int in_port, int *out_port);
int net_nat_static(NetNat *box, int out_port, uint32_t in_ip, int in_port);
int net_nat_in(const NetNat *box, uint32_t dst, int dport, uint32_t *in_ip, int *in_port);
int net_nat_line(const char *id, char *out, int cap);

#endif
