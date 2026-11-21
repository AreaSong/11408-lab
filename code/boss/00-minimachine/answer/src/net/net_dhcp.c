#include "net/net_dhcp.h"

#include "net/net_ip.h"

#include <stdio.h>
#include <string.h>

static const char *k_step[] = {"DISCOVER", "OFFER", "REQUEST", "ACK"};

int net_dhcp_accept(int *state, const char *msg, uint32_t *addr) {
    if (state == NULL || msg == NULL || *state < 0 || *state > 3 || strcmp(msg, k_step[*state]) != 0) {
        return -1;
    }
    (*state)++;
    if (*state == 4 && addr != NULL && net_ip_parse("192.168.1.10", addr) != 0) {
        return -1;
    }
    return 0;
}

int net_dhcp_line(char *out, int cap) {
    uint32_t addr = 0;
    int state = 0;
    char text[16];
    int i;
    int n;
    for (i = 0; i < 4; i++) {
        if (net_dhcp_accept(&state, k_step[i], &addr) != 0) {
            return -1;
        }
    }
    if (state != 4 || net_ip_fmt(addr, text, (int)sizeof text) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s %s %s %s addr=%s", k_step[0], k_step[1], k_step[2], k_step[3], text);
    return n > 0 && n < cap ? 0 : -1;
}
