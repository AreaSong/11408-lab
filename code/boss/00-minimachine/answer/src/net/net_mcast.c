#include "net/net_mcast.h"

#include "net/net_ip.h"

#include <stdio.h>

int net_mcast_mac(const char *ip, char *mac, int cap, int *alias) {
    uint32_t addr = 0;
    int n;
    if (net_ip_parse(ip, &addr) != 0 || net_ip_class(addr) != 'D' || mac == NULL || alias == NULL) {
        return -1;
    }
    n = snprintf(mac, (size_t)cap, "01-00-5e-%02x-%02x-%02x", (addr >> 16) & 0x7fu, (addr >> 8) & 0xffu,
                 addr & 0xffu);
    *alias = 32;
    return n > 0 && n < cap ? 0 : -1;
}
