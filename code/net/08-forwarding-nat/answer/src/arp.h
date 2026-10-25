#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_SLOTS 64
#define VLAN_MAX 16

typedef struct {
    int used;
    uint32_t ip;
    uint8_t mac[6];
} ArpSlot;

typedef struct {
    ArpSlot slot[ARP_SLOTS];
} ArpCache;

typedef struct {
    int used;
    int id;
    uint32_t ip;
    uint8_t mac[6];
} VlanHost;

/* 目的不在本网段时返回网关，并让 remote = 1。ARP 过不了路由器。 */
uint32_t arp_target(uint32_t host, int prefix, uint32_t dst, uint32_t gw, int *remote);

int arp_put(ArpCache *cache, uint32_t ip, const uint8_t mac[6]);
int arp_get(const ArpCache *cache, uint32_t ip, uint8_t mac[6]);

int vlan_add(VlanHost *tab, int *n, int id, uint32_t ip, const uint8_t mac[6]);
int vlan_find(const VlanHost *tab, int n, uint32_t ip);

#endif
