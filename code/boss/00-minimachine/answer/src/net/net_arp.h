#ifndef NET_ARP_H
#define NET_ARP_H

#include <stdint.h>

/* ARP 解析的是下一跳，不是最终目的。请求广播，响应单播。 */
#define NET_ARP_MAX 8

typedef struct {
    uint32_t ip;
    char mac[24];
    int used;
} NetArpSlot;

typedef struct {
    NetArpSlot slot[NET_ARP_MAX];
    int n;
} NetArp;

int net_arp_who(uint32_t dst, uint32_t mine, int prefix, uint32_t gw, uint32_t *who);
int net_arp_put(NetArp *cache, uint32_t ip, const char *mac);
int net_arp_get(const NetArp *cache, uint32_t ip, char *mac, int cap);
int net_arp_2011(char *target, int tc, char *req, int rc, char *mac, int mc);
int net_arp_2024(const char *ip, int *vlan, int *show);
int net_arp_line(const char *id, char *out, int cap);

#endif
