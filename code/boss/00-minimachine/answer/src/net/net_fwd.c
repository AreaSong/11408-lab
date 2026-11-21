#include "net/net_fwd.h"

#include "bytes.h"
#include "net/net_ip.h"

#include <stdio.h>
#include <string.h>

void net_fib_init(NetFib *fib) {
    if (fib != NULL) {
        fib->n = 0;
    }
}

int net_fib_add(NetFib *fib, const char *cidr, const char *hop, const char *ifname) {
    uint32_t net = 0;
    uint32_t gw = 0;
    int prefix = 0;
    if (fib == NULL || fib->n >= NET_FWD_MAX || net_ip_cidr(cidr, &net, &prefix) != 0 || ifname == NULL) {
        return -1;
    }
    if (hop != NULL && strcmp(hop, "直连") != 0 && strcmp(hop, "-") != 0) {
        if (net_ip_parse(hop, &gw) != 0) {
            return -1;
        }
    }
    fib->row[fib->n].net = net;
    fib->row[fib->n].prefix = prefix;
    fib->row[fib->n].hop = gw;
    snprintf(fib->row[fib->n].ifname, sizeof fib->row[0].ifname, "%s", ifname);
    fib->n++;
    return 0;
}

static int load_rows(NetFib *fib, const char *const *cidr, const char *const *hop, const char *const *iface,
                     int n) {
    int i;
    net_fib_init(fib);
    for (i = 0; i < n; i++) {
        if (net_fib_add(fib, cidr[i], hop[i], iface[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

int net_fib_load(NetFib *fib, const char *id) {
    if (strcmp(id, "2013-47") == 0) {
        const char *cidr[] = {"153.14.5.0/24", "194.17.20.0/23", "194.17.20.128/25"};
        const char *hop[] = {"153.14.3.2", "194.17.24.2", "直连"};
        const char *iface[] = {"S0", "S1", "E0"};
        return load_rows(fib, cidr, hop, iface, 3);
    }
    if (strcmp(id, "2009-47") == 0) {
        const char *cidr[] = {"202.118.1.0/25", "202.118.1.128/25", "202.118.3.2/32", "0.0.0.0/0"};
        const char *hop[] = {"直连", "直连", "202.118.2.2", "202.118.2.2"};
        const char *iface[] = {"E1", "E2", "L0", "L0"};
        return load_rows(fib, cidr, hop, iface, 4);
    }
    if (strcmp(id, "2014-43") == 0) {
        const char *cidr[] = {"192.1.1.0/24", "192.1.6.0/23", "192.1.5.0/24"};
        const char *hop[] = {"直连", "10.1.1.2", "10.1.1.10"};
        const char *iface[] = {"E0", "L0", "L1"};
        return load_rows(fib, cidr, hop, iface, 3);
    }
    if (strcmp(id, "demo") == 0) {
        const char *cidr[] = {"10.0.0.0/24", "10.1.0.0/24"};
        const char *hop[] = {"直连", "直连"};
        const char *iface[] = {"E0", "E1"};
        return load_rows(fib, cidr, hop, iface, 2);
    }
    return -1;
}

int net_fib_find(const NetFib *fib, uint32_t dst, char *hop, int hcap, char *ifname, int icap) {
    int best = -1;
    int best_len = -1;
    int i;
    if (fib == NULL || hop == NULL || ifname == NULL) {
        return -1;
    }
    for (i = 0; i < fib->n; i++) {
        uint32_t mask = net_ip_mask(fib->row[i].prefix);
        if ((dst & mask) == (fib->row[i].net & mask) && fib->row[i].prefix > best_len) {
            best = i;
            best_len = fib->row[i].prefix;
        }
    }
    if (best < 0) {
        return -1;
    }
    if (fib->row[best].hop == 0) {
        snprintf(hop, (size_t)hcap, "直连");
    } else if (net_ip_fmt(fib->row[best].hop, hop, hcap) != 0) {
        return -1;
    }
    snprintf(ifname, (size_t)icap, "%s", fib->row[best].ifname);
    return best_len;
}

static int slice_diff(const unsigned char *a, const unsigned char *b, int off, int len) {
    return memcmp(a + off, b + off, (size_t)len) != 0;
}

int net_fwd_diff(const unsigned char before[20], const unsigned char after[20], char *out, int cap) {
    static const struct {
        int off;
        int len;
        const char *name;
    } field[] = {
        {12, 4, "源地址"}, {16, 4, "目的地址"}, {8, 1, "TTL"},
        {2, 2, "总长度"},  {6, 2, "标志和片偏移"}, {10, 2, "首部校验和"},
    };
    int used = 0;
    int i;
    int any = 0;
    if (before == NULL || after == NULL || out == NULL) {
        return -1;
    }
    out[0] = '\0';
    for (i = 0; i < 6; i++) {
        int n;
        if (!slice_diff(before, after, field[i].off, field[i].len)) {
            continue;
        }
        n = snprintf(out + used, (size_t)cap - (size_t)used, "%s%s", any ? "、" : "", field[i].name);
        if (n < 0 || used + n >= cap) {
            return -1;
        }
        used += n;
        any = 1;
    }
    return any ? 0 : -1;
}

static void hdr_2011(unsigned char hdr[20]) {
    static const unsigned char raw[20] = {0x45, 0x00, 0x01, 0xef, 0x11, 0x3b, 0x40, 0x00, 0x80, 0x06,
                                           0xba, 0x9d, 0x0a, 0x02, 0x80, 0x64, 0x40, 0xaa, 0x62, 0x20};
    memcpy(hdr, raw, 20);
}

static void hdr_echo(unsigned char hdr[20], int ttl) {
    static const unsigned char raw[20] = {0x45, 0x00, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00, 0x40, 0x01,
                                           0x66, 0xde, 0x0a, 0x00, 0x00, 0x01, 0x0a, 0x00, 0x00, 0x02};
    memcpy(hdr, raw, 20);
    hdr[8] = (unsigned char)ttl;
}

int net_fwd_2011(char *src, int sc, int *ttl, unsigned *cksum, char *changed, int ccap) {
    unsigned char before[20];
    unsigned char after[20];
    hdr_2011(before);
    memcpy(after, before, 20);
    if (net_ip_set(after, 127, 1, 0x650c7b0fu) != 0) {
        return -1;
    }
    if (src != NULL && net_ip_fmt(read_be32(after + 12), src, sc) != 0) {
        return -1;
    }
    if (ttl != NULL) {
        *ttl = after[8];
    }
    if (cksum != NULL) {
        *cksum = read_be16(after + 10);
    }
    return changed == NULL ? 0 : net_fwd_diff(before, after, changed, ccap);
}

int net_fwd_hop(unsigned char hdr[20], int *icmp_type, int *icmp_code) {
    if (hdr == NULL || icmp_type == NULL || icmp_code == NULL) {
        return -1;
    }
    if (hdr[8] <= 1) {
        *icmp_type = 11;
        *icmp_code = 0;
        return 1;
    }
    *icmp_type = 0;
    *icmp_code = 0;
    return net_ip_set(hdr, hdr[8] - 1, 0, 0);
}

static int line_ttl(const char *id, char *out, int cap) {
    unsigned char before[20];
    unsigned char after[20];
    char changed[64];
    int type = 0;
    int code = 0;
    int n;
    hdr_echo(before, strcmp(id, "zero") == 0 ? 1 : 64);
    memcpy(after, before, 20);
    if (net_fwd_hop(after, &type, &code) < 0) {
        return -1;
    }
    if (type == 11) {
        n = snprintf(out, (size_t)cap, "icmp=%d code=%d", type, code);
        return n > 0 && n < cap ? 0 : -1;
    }
    if (net_fwd_diff(before, after, changed, (int)sizeof changed) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "ttl=%d cksum=%04x changed=%s", after[8], read_be16(after + 10), changed);
    return n > 0 && n < cap ? 0 : -1;
}

int net_fwd_line(const char *id, char *out, int cap) {
    char src[16];
    char changed[64];
    int ttl = 0;
    unsigned cksum = 0;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "dec") == 0 || strcmp(id, "zero") == 0) {
        return line_ttl(id, out, cap);
    }
    if (strcmp(id, "2011-47") != 0 ||
        net_fwd_2011(src, (int)sizeof src, &ttl, &cksum, changed, (int)sizeof changed) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "src=%s ttl=%d cksum=%04x changed=%s", src, ttl, cksum, changed);
    return n > 0 && n < cap ? 0 : -1;
}

int net_route_line(const char *id, const char *dst, char *out, int cap) {
    NetFib fib;
    uint32_t addr = 0;
    char hop[16];
    char iface[8];
    int n;
    if (net_fib_load(&fib, id) != 0 || net_ip_parse(dst, &addr) != 0 ||
        net_fib_find(&fib, addr, hop, (int)sizeof hop, iface, (int)sizeof iface) < 0) {
        return -1;
    }
    if (strcmp(id, "2014-43") == 0) {
        n = snprintf(out, (size_t)cap, "hop=%s if=%s ttl=61", hop, iface);
    } else {
        n = snprintf(out, (size_t)cap, "hop=%s if=%s", hop, iface);
    }
    return n > 0 && n < cap ? 0 : -1;
}
