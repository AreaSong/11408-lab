#include "arp.h"

#include "addr.h"

#include <string.h>

uint32_t arp_target(uint32_t host, int prefix, uint32_t dst, uint32_t gw, int *remote) {
    uint32_t mask = prefix_mask(prefix);

    if ((host & mask) == (dst & mask)) {
        *remote = 0;
        return dst;
    }
    *remote = 1;
    return gw;
}

static unsigned ip_hash(uint32_t ip) {
    return (ip * 16777619u) % ARP_SLOTS;
}

int arp_get(const ArpCache *cache, uint32_t ip, uint8_t mac[6]) {
    unsigned h = ip_hash(ip);
    int i;

    for (i = 0; i < ARP_SLOTS; i++) {
        int idx = (int)((h + (unsigned)i) % ARP_SLOTS);
        const ArpSlot *slot = &cache->slot[idx];

        if (!slot->used) {
            return 0;
        }
        if (slot->ip == ip) {
            memcpy(mac, slot->mac, 6);
            return 1;
        }
    }
    return 0;
}

int arp_put(ArpCache *cache, uint32_t ip, const uint8_t mac[6]) {
    unsigned h = ip_hash(ip);
    int i;
    int empty = -1;

    for (i = 0; i < ARP_SLOTS; i++) {
        int idx = (int)((h + (unsigned)i) % ARP_SLOTS);

        if (!cache->slot[idx].used) {
            empty = idx;
            break;
        }
        if (cache->slot[idx].ip == ip) {
            memcpy(cache->slot[idx].mac, mac, 6);
            return 1;
        }
    }
    if (empty < 0) {
        return 0;
    }
    cache->slot[empty].used = 1;
    cache->slot[empty].ip = ip;
    memcpy(cache->slot[empty].mac, mac, 6);
    return 1;
}

int vlan_add(VlanHost *tab, int *n, int id, uint32_t ip, const uint8_t mac[6]) {
    int i;

    if (id <= 0) {
        return 0;
    }
    for (i = 0; i < *n; i++) {
        if (tab[i].ip == ip) {
            tab[i].id = id;
            memcpy(tab[i].mac, mac, 6);
            return 1;
        }
    }
    if (*n >= VLAN_MAX) {
        return 0;
    }
    tab[*n].used = 1;
    tab[*n].id = id;
    tab[*n].ip = ip;
    memcpy(tab[*n].mac, mac, 6);
    (*n)++;
    return 1;
}

int vlan_find(const VlanHost *tab, int n, uint32_t ip) {
    int i;

    for (i = 0; i < n; i++) {
        if (tab[i].used && tab[i].ip == ip) {
            return tab[i].id;
        }
    }
    return 0;
}
