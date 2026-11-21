#include "net/net_arp.h"

#include "net/net_ip.h"

#include <stdio.h>
#include <string.h>

int net_arp_who(uint32_t dst, uint32_t mine, int prefix, uint32_t gw, uint32_t *who) {
    if (who == NULL || prefix < 0 || prefix > 32) {
        return -1;
    }
    if ((dst & net_ip_mask(prefix)) == (mine & net_ip_mask(prefix))) {
        *who = dst;
    } else {
        *who = gw;
    }
    return 0;
}

int net_arp_put(NetArp *cache, uint32_t ip, const char *mac) {
    int i;
    if (cache == NULL || mac == NULL) {
        return -1;
    }
    for (i = 0; i < cache->n; i++) {
        if (cache->slot[i].used && cache->slot[i].ip == ip) {
            snprintf(cache->slot[i].mac, sizeof cache->slot[i].mac, "%s", mac);
            return 0;
        }
    }
    if (cache->n >= NET_ARP_MAX) {
        return -1;
    }
    cache->slot[cache->n].ip = ip;
    cache->slot[cache->n].used = 1;
    snprintf(cache->slot[cache->n].mac, sizeof cache->slot[0].mac, "%s", mac);
    cache->n++;
    return 0;
}

int net_arp_get(const NetArp *cache, uint32_t ip, char *mac, int cap) {
    int i;
    if (cache == NULL || mac == NULL) {
        return -1;
    }
    for (i = 0; i < cache->n; i++) {
        if (cache->slot[i].used && cache->slot[i].ip == ip) {
            snprintf(mac, (size_t)cap, "%s", cache->slot[i].mac);
            return 0;
        }
    }
    return -1;
}

int net_arp_2011(char *target, int tc, char *req, int rc, char *mac, int mc) {
    uint32_t mine = 0;
    uint32_t dst = 0;
    uint32_t gw = 0;
    uint32_t who = 0;
    if (net_ip_parse("10.2.128.100", &mine) != 0 || net_ip_parse("64.170.98.32", &dst) != 0 ||
        net_ip_parse("10.2.128.1", &gw) != 0 || net_arp_who(dst, mine, 24, gw, &who) != 0) {
        return -1;
    }
    if (net_ip_fmt(who, target, tc) != 0) {
        return -1;
    }
    snprintf(req, (size_t)rc, "ff-ff-ff-ff-ff-ff");
    snprintf(mac, (size_t)mc, "00-21-27-21-51-ee");
    return who == gw ? 0 : -1;
}

int net_arp_2024(const char *ip, int *vlan, int *show) {
    static const struct {
        const char *ip;
        int vlan;
    } row[] = {
        {"192.168.3.81", 1},
        {"192.168.3.91", 1},
        {"192.168.3.125", 1},
        {"192.168.3.129", 3},
    };
    int i;
    if (ip == NULL || vlan == NULL || show == NULL) {
        return -1;
    }
    for (i = 0; i < 4; i++) {
        if (strcmp(row[i].ip, ip) == 0) {
            *vlan = row[i].vlan;
            *show = row[i].vlan == 1;
            return 0;
        }
    }
    return -1;
}

static int line_2024(char *out, int cap) {
    static const char *ip[] = {"192.168.3.81", "192.168.3.91", "192.168.3.125", "192.168.3.129"};
    int used = 0;
    int i;
    out[0] = '\0';
    for (i = 0; i < 4; i++) {
        int vlan = 0;
        int show = 0;
        int n;
        if (net_arp_2024(ip[i], &vlan, &show) != 0) {
            return -1;
        }
        n = snprintf(out + used, (size_t)cap - (size_t)used, "%s%s vlan=%d show=%d", i ? "; " : "", ip[i], vlan,
                     show);
        if (n < 0 || used + n >= cap) {
            return -1;
        }
        used += n;
    }
    return 0;
}

int net_arp_line(const char *id, char *out, int cap) {
    char target[16];
    char req[24];
    char mac[24];
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2024-35") == 0) {
        return line_2024(out, cap);
    }
    if (strcmp(id, "2011-47") != 0 ||
        net_arp_2011(target, (int)sizeof target, req, (int)sizeof req, mac, (int)sizeof mac) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "target=%s req=%s mac=%s", target, req, mac);
    return n > 0 && n < cap ? 0 : -1;
}
