#include "net/net_stack.h"

#include "bytes.h"
#include "net/net_arp.h"
#include "net/net_dhcp.h"
#include "net/net_fwd.h"
#include "net/net_ip.h"
#include "net/net_nat.h"

#include <stdio.h>
#include <string.h>

static int add_line(char *out, int cap, int *used, const char *line) {
    int n = snprintf(out + *used, (size_t)cap - (size_t)*used, "%s\n", line);
    if (n < 0 || *used + n >= cap) {
        return -1;
    }
    *used += n;
    return 0;
}

static int arp_line(char *buf, int cap) {
    uint32_t mine = 0;
    uint32_t dst = 0;
    uint32_t gw = 0;
    uint32_t who = 0;
    NetArp cache;
    char ip[16];
    char mac[24];
    if (net_ip_parse("10.0.0.2", &mine) != 0 || net_ip_parse("10.1.0.2", &dst) != 0 ||
        net_ip_parse("10.0.0.1", &gw) != 0 || net_arp_who(dst, mine, 24, gw, &who) != 0) {
        return -1;
    }
    memset(&cache, 0, sizeof cache);
    if (net_arp_put(&cache, who, "02-00-00-00-01") != 0 || net_arp_get(&cache, who, mac, (int)sizeof mac) != 0 ||
        net_ip_fmt(who, ip, (int)sizeof ip) != 0) {
        return -1;
    }
    snprintf(buf, (size_t)cap, "arp target=%s req=ff-ff-ff-ff-ff-ff mac=%s", ip, mac);
    return who == gw ? 0 : -1;
}

static int fwd_line(char *buf, int cap) {
    NetFib fib;
    unsigned char hdr[20];
    uint32_t src = 0;
    uint32_t dst = 0;
    char hop[16];
    char iface[8];
    int type = 0;
    int code = 0;
    if (net_ip_parse("10.0.0.2", &src) != 0 || net_ip_parse("10.1.0.2", &dst) != 0 || net_fib_load(&fib, "demo") != 0) {
        return -1;
    }
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 0x45;
    write_be16(hdr + 2, 28);
    hdr[9] = 1;
    net_ip_set(hdr, 64, 1, src);
    write_be32(hdr + 16, dst);
    net_ip_set(hdr, 64, 0, 0);
    if (net_fib_find(&fib, dst, hop, (int)sizeof hop, iface, (int)sizeof iface) < 0 ||
        net_fwd_hop(hdr, &type, &code) != 0) {
        return -1;
    }
    snprintf(buf, (size_t)cap, "fwd if=%s ttl=%d cksum=%04x", iface, hdr[8], read_be16(hdr + 10));
    return 0;
}

static int frag_line(char *buf, int cap) {
    NetFrag piece[4];
    int n = net_ip_frag(1500, 20, 800, 0, piece, 4);
    if (n != 2) {
        return -1;
    }
    snprintf(buf, (size_t)cap, "frag n=%d off=%d,%d len=%d,%d", n, piece[0].offset, piece[1].offset, piece[0].total,
             piece[1].total);
    return 0;
}

static int icmp_line(char *miss, int mcap, char *zero, int zcap) {
    NetFib fib;
    unsigned char hdr[20];
    uint32_t dst = 0;
    char hop[16];
    char iface[8];
    int type = 0;
    int code = 0;
    if (net_fib_load(&fib, "demo") != 0 || net_ip_parse("8.8.8.8", &dst) != 0) {
        return -1;
    }
    if (net_fib_find(&fib, dst, hop, (int)sizeof hop, iface, (int)sizeof iface) >= 0) {
        return -1;
    }
    snprintf(miss, (size_t)mcap, "miss icmp=3 code=0");
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 0x45;
    hdr[8] = 1;
    if (net_fwd_hop(hdr, &type, &code) != 1) {
        return -1;
    }
    snprintf(zero, (size_t)zcap, "zero icmp=%d code=%d", type, code);
    return 0;
}

static int nat_line(char *buf, int cap) {
    NetNat box;
    uint32_t pub = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    int pa = 0;
    int pb = 0;
    if (net_ip_parse("203.0.113.10", &pub) != 0 || net_ip_parse("10.0.0.2", &a) != 0 ||
        net_ip_parse("10.0.0.3", &b) != 0) {
        return -1;
    }
    net_nat_init(&box, pub);
    if (net_nat_map(&box, a, 4000, &pa) != 0 || net_nat_map(&box, b, 4000, &pb) != 0) {
        return -1;
    }
    snprintf(buf, (size_t)cap, "nat %d %d", pa, pb);
    return pa == 4000 && pb == 4001 ? 0 : -1;
}

int net_stack_line(char *out, int cap) {
    char line[96];
    char miss[32];
    char zero[32];
    char dhcp[80];
    int used = 0;
    if (out == NULL || arp_line(line, (int)sizeof line) != 0 || add_line(out, cap, &used, line) != 0) {
        return -1;
    }
    if (fwd_line(line, (int)sizeof line) != 0 || add_line(out, cap, &used, line) != 0) {
        return -1;
    }
    if (frag_line(line, (int)sizeof line) != 0 || add_line(out, cap, &used, line) != 0) {
        return -1;
    }
    if (icmp_line(miss, (int)sizeof miss, zero, (int)sizeof zero) != 0 || add_line(out, cap, &used, miss) != 0 ||
        add_line(out, cap, &used, zero) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "ping type=0 from=10.1.0.2");
    if (add_line(out, cap, &used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "hop 1 type=11 from=10.0.0.1");
    if (add_line(out, cap, &used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "hop 2 type=0 from=10.1.0.2");
    if (add_line(out, cap, &used, line) != 0 || nat_line(line, (int)sizeof line) != 0 ||
        add_line(out, cap, &used, line) != 0 || net_dhcp_line(dhcp, (int)sizeof dhcp) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "dhcp %s", dhcp);
    return add_line(out, cap, &used, line);
}
