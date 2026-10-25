#include "ip.h"

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
    snprintf(out, (size_t)cap, "%u.%u.%u.%u", (ip >> 24) & 255u, (ip >> 16) & 255u,
             (ip >> 8) & 255u, ip & 255u);
}

void block_fmt(Block b, char *out, int cap) {
    char ip[16];

    ip_fmt(b.base, ip, (int)sizeof ip);
    snprintf(out, (size_t)cap, "%s/%d", ip, b.prefix);
}

uint32_t prefix_mask(int prefix) {
    if (prefix <= 0) {
        return 0;
    }
    if (prefix >= 32) {
        return 0xffffffffu;
    }
    return ~((1u << (32 - prefix)) - 1u);
}

int mask_to_prefix(uint32_t mask, int *prefix) {
    uint32_t host = ~mask;
    int bits = 0;

    /* host 必须是低位连续的 1（或 0）。不连续的掩码不能写成前缀。 */
    if ((host & (host + 1u)) != 0) {
        return 0;
    }
    while ((mask & 0x80000000u) != 0) {
        bits++;
        mask <<= 1;
    }
    if (mask != 0) {
        return 0;
    }
    *prefix = bits;
    return 1;
}

uint32_t ip_network(uint32_t addr, int prefix) {
    return addr & prefix_mask(prefix);
}

uint32_t ip_broadcast(uint32_t addr, int prefix) {
    if (prefix <= 0) {
        return 0xffffffffu;
    }
    if (prefix >= 32) {
        return addr;
    }
    return ip_network(addr, prefix) | ~prefix_mask(prefix);
}

int ip_aligned(uint32_t addr, int prefix) {
    return ip_network(addr, prefix) == addr;
}

uint64_t ip_space(int prefix) {
    if (prefix <= 0) {
        return 1ull << 32;
    }
    if (prefix >= 32) {
        return 1;
    }
    return 1ull << (32 - prefix);
}

uint64_t ip_usable(int prefix) {
    if (prefix >= 32) {
        return 1;
    }
    if (prefix == 31) {
        return 0;
    }
    return ip_space(prefix) - 2ull;
}

uint32_t block_last(Block b) {
    if (b.prefix <= 0) {
        return 0xffffffffu;
    }
    if (b.prefix >= 32) {
        return b.base;
    }
    return b.base + (uint32_t)(ip_space(b.prefix) - 1ull);
}

int block_aligned(Block b) {
    return b.prefix >= 0 && b.prefix <= 32 && ip_aligned(b.base, b.prefix);
}

static int in_block(uint32_t addr, uint32_t net, int prefix) {
    return ip_network(addr, prefix) == net;
}

void ip_classify(uint32_t addr, IpKind *kind, IpScope *scope) {
    unsigned first = (addr >> 24) & 255u;

    *scope = IP_SCOPE_PUBLIC;
    if (addr == 0xffffffffu) {
        *kind = IP_KIND_LIMITED;
        *scope = IP_SCOPE_SPECIAL;
        return;
    }
    if (first == 0) {
        *kind = IP_KIND_THIS;
        *scope = IP_SCOPE_SPECIAL;
        return;
    }
    if (first == 127) {
        *kind = IP_KIND_LOOP;
        *scope = IP_SCOPE_SPECIAL;
        return;
    }
    if (first < 128) {
        *kind = IP_KIND_A;
    } else if (first < 192) {
        *kind = IP_KIND_B;
    } else if (first < 224) {
        *kind = IP_KIND_C;
    } else if (first < 240) {
        *kind = IP_KIND_D;
        *scope = IP_SCOPE_SPECIAL;
        return;
    } else {
        *kind = IP_KIND_E;
        *scope = IP_SCOPE_SPECIAL;
        return;
    }
    if (in_block(addr, 0x0a000000u, 8) || in_block(addr, 0xac100000u, 12) ||
        in_block(addr, 0xc0a80000u, 16)) {
        *scope = IP_SCOPE_PRIVATE;
    } else if (in_block(addr, 0xa9fe0000u, 16)) {
        *scope = IP_SCOPE_SPECIAL;
    }
}

const char *ip_kind_name(IpKind kind) {
    switch (kind) {
    case IP_KIND_A:
        return "A 类";
    case IP_KIND_B:
        return "B 类";
    case IP_KIND_C:
        return "C 类";
    case IP_KIND_D:
        return "D 类";
    case IP_KIND_E:
        return "E 类";
    case IP_KIND_THIS:
        return "本网络";
    case IP_KIND_LOOP:
        return "环回";
    case IP_KIND_LIMITED:
        return "受限广播";
    }
    return "未知";
}

const char *ip_proto_name(unsigned protocol) {
    switch (protocol) {
    case 1:
        return "ICMP";
    case 6:
        return "TCP";
    case 17:
        return "UDP";
    default:
        return "其他";
    }
}
