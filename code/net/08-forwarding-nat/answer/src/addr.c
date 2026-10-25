#include "addr.h"

#include <stdio.h>
#include <string.h>

int ip_parse(const char *text, uint32_t *out) {
    unsigned a;
    unsigned b;
    unsigned c;
    unsigned d;
    int n = 0;

    if (sscanf(text, "%u.%u.%u.%u%n", &a, &b, &c, &d, &n) != 4) {
        return 0;
    }
    if (text[n] != '\0' || a > 255u || b > 255u || c > 255u || d > 255u) {
        return 0;
    }
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 1;
}

void ip_fmt(uint32_t ip, char *out, int cap) {
    snprintf(out, (size_t)cap, "%u.%u.%u.%u", (ip >> 24) & 255u, (ip >> 16) & 255u, (ip >> 8) & 255u,
             ip & 255u);
}

int prefix_parse(const char *text, uint32_t *net, int *prefix) {
    char ip[32];
    const char *slash = strchr(text, '/');
    unsigned p;
    int n = 0;
    int len;

    if (slash == NULL) {
        return 0;
    }
    len = (int)(slash - text);
    if (len <= 0 || len >= (int)sizeof ip) {
        return 0;
    }
    memcpy(ip, text, (size_t)len);
    ip[len] = '\0';
    if (!ip_parse(ip, net)) {
        return 0;
    }
    if (sscanf(slash + 1, "%u%n", &p, &n) != 1 || (slash + 1)[n] != '\0' || p > 32u) {
        return 0;
    }
    *prefix = (int)p;
    return 1;
}

void prefix_fmt(uint32_t net, int prefix, char *out, int cap) {
    char ip[16];

    ip_fmt(net, ip, (int)sizeof ip);
    snprintf(out, (size_t)cap, "%s/%d", ip, prefix);
}

uint32_t prefix_mask(int prefix) {
    if (prefix <= 0) {
        return 0;
    }
    if (prefix >= 32) {
        return 0xffffffffu;
    }
    return 0xffffffffu << (32 - prefix);
}

int ip_aligned(uint32_t addr, int prefix) {
    uint32_t mask = prefix_mask(prefix);

    return (addr & mask) == addr;
}

int mac_parse(const char *text, uint8_t mac[6]) {
    unsigned v[6];
    int n = 0;
    int i;

    if (sscanf(text, "%x-%x-%x-%x-%x-%x%n", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &n) != 6) {
        return 0;
    }
    if (text[n] != '\0') {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        if (v[i] > 255u) {
            return 0;
        }
        mac[i] = (uint8_t)v[i];
    }
    return 1;
}

void mac_fmt(const uint8_t mac[6], char *out, int cap) {
    snprintf(out, (size_t)cap, "%02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
}

void bits8(unsigned value, char out[9]) {
    int i;

    for (i = 0; i < 8; i++) {
        out[i] = (char)(((value >> (7 - i)) & 1u) ? '1' : '0');
    }
    out[8] = '\0';
}
