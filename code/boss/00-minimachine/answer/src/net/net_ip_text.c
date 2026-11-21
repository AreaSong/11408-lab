#include "net/net_ip.h"

#include <stdio.h>
#include <string.h>

static int join_pre(uint32_t addr, int prefix, char *out, int cap) {
    char ip[16];
    int n;
    if (net_ip_fmt(addr, ip, (int)sizeof ip) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s/%d", ip, prefix);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_2010(char *out, int cap) {
    int each = 0;
    int count = net_ip_fixed(24, 29, &each);
    int n = snprintf(out, (size_t)cap, "subnets=%d each=%d", count, each);
    return count == 32 && n > 0 && n < cap ? 0 : -1;
}

static int line_2009(char *out, int cap) {
    uint32_t base = 0;
    uint32_t second;
    uint32_t agg = 0;
    int pre = 0;
    int each = 0;
    int filled = 0;
    int out_pre = 0;
    uint32_t nets[2];
    int pres[2];
    char a[24];
    char b[24];
    char c[24];
    int n;
    if (net_ip_cidr("202.118.1.0/24", &base, &pre) != 0 || net_ip_fixed(24, 25, &each) != 2) {
        return -1;
    }
    second = base + (1u << 7);
    nets[0] = base;
    nets[1] = second;
    pres[0] = pres[1] = 25;
    if (net_ip_agg(nets, pres, 2, &agg, &out_pre, &filled) != 0) {
        return -1;
    }
    if (join_pre(base, 25, a, (int)sizeof a) != 0 || join_pre(second, 25, b, (int)sizeof b) != 0 ||
        join_pre(agg, out_pre, c, (int)sizeof c) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s %s hosts=%d agg=%s", a, b, each, c);
    return filled && n > 0 && n < cap ? 0 : -1;
}

static int line_2018(char *out, int cap) {
    static const char *block[] = {"35.230.32.0/21", "35.230.40.0/21", "35.230.48.0/21", "35.230.56.0/21"};
    uint32_t nets[4];
    int pres[4];
    uint32_t agg = 0;
    int out_pre = 0;
    int filled = 0;
    char text[24];
    int i;
    int n;
    for (i = 0; i < 4; i++) {
        if (net_ip_cidr(block[i], &nets[i], &pres[i]) != 0) {
            return -1;
        }
    }
    if (net_ip_agg(nets, pres, 4, &agg, &out_pre, &filled) != 0 ||
        join_pre(agg, out_pre, text, (int)sizeof text) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s full=%d", text, filled);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_2019(char *out, int cap) {
    uint32_t base = 0;
    uint32_t nets[5];
    int pres[5];
    int pre = 0;
    int hosts;
    int n;
    if (net_ip_cidr("101.200.16.0/20", &base, &pre) != 0) {
        return -1;
    }
    hosts = net_ip_vlsm(base, pre, 5, nets, pres, 5);
    n = snprintf(out, (size_t)cap, "min=%d", hosts);
    return hosts == 254 && n > 0 && n < cap ? 0 : -1;
}

static int line_one(const char *cidr, char *text, int cap, int *hosts) {
    uint32_t addr = 0;
    uint32_t bcast;
    int pre = 0;
    if (net_ip_cidr(cidr, &addr, &pre) != 0) {
        return -1;
    }
    bcast = net_ip_bcast(addr, pre);
    *hosts = net_ip_hosts(pre);
    if (net_ip_fmt(bcast, text, cap) != 0) {
        return -1;
    }
    return 0;
}

static int line_2012(char *out, int cap) {
    uint32_t addr = 0;
    int pre = 0;
    char net[24];
    char bcast[16];
    int hosts = 0;
    int n;
    if (net_ip_parse("180.80.77.55", &addr) != 0) {
        return -1;
    }
    pre = 22;
    if (join_pre(net_ip_net(addr, pre), pre, net, (int)sizeof net) != 0 ||
        net_ip_fmt(net_ip_bcast(addr, pre), bcast, (int)sizeof bcast) != 0) {
        return -1;
    }
    hosts = net_ip_hosts(pre);
    n = snprintf(out, (size_t)cap, "net=%s bcast=%s hosts=%d", net, bcast, hosts);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_2011_38(char *out, int cap) {
    char bcast[16];
    int hosts = 0;
    int n;
    if (line_one("192.168.4.0/30", bcast, (int)sizeof bcast, &hosts) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "bcast=%s hosts=%d", bcast, hosts);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_pair(const char *a, const char *b, char *out, int cap) {
    uint32_t nets[2];
    int pres[2];
    uint32_t agg = 0;
    int out_pre = 0;
    int filled = 0;
    char text[24];
    int n;
    if (net_ip_cidr(a, &nets[0], &pres[0]) != 0 || net_ip_cidr(b, &nets[1], &pres[1]) != 0 ||
        net_ip_agg(nets, pres, 2, &agg, &out_pre, &filled) != 0 ||
        join_pre(agg, out_pre, text, (int)sizeof text) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s full=%d", text, filled);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_2021(char *out, int cap) {
    static const char *cand[] = {"192.168.9.0/25", "192.168.9.0/26", "192.168.9.192/26", "192.168.9.192/27"};
    uint32_t known = 0;
    int kp = 0;
    int used = 0;
    int i;
    if (net_ip_cidr("192.168.9.128/26", &known, &kp) != 0) {
        return -1;
    }
    out[0] = '\0';
    for (i = 0; i < 4; i++) {
        uint32_t addr = 0;
        uint32_t other = 0;
        int cp = 0;
        int op = 0;
        int ok = 0;
        char piece[40];
        char third[24];
        int n;
        if (net_ip_cidr(cand[i], &addr, &cp) != 0 ||
            net_ip_third(known, kp, addr, cp, 3, &other, &op, &ok) != 0) {
            return -1;
        }
        if (ok && join_pre(other, op, third, (int)sizeof third) != 0) {
            return -1;
        }
        n = snprintf(piece, sizeof piece, "%s%s %s", i ? "; " : "", cand[i], ok ? third : "no");
        if (n < 0 || used + n >= cap) {
            return -1;
        }
        memcpy(out + used, piece, (size_t)n + 1);
        used += n;
    }
    return 0;
}

static int line_2018_host(char *out, int cap) {
    uint32_t low = 0;
    uint32_t high = 0;
    int pre = 0;
    char bcast[16];
    char net[24];
    int left;
    int n;
    if (net_ip_parse("192.168.1.1", &low) != 0 || net_ip_parse("192.168.1.208", &high) != 0) {
        return -1;
    }
    pre = 25;
    if (net_ip_fmt(net_ip_bcast(low, pre), bcast, (int)sizeof bcast) != 0 ||
        join_pre(net_ip_net(high, pre), pre, net, (int)sizeof net) != 0) {
        return -1;
    }
    left = net_ip_hosts(pre) - (208 - 129 + 1) - 1;
    n = snprintf(out, (size_t)cap, "bcast=%s net=%s left=%d", bcast, net, left);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_class(const char *text, char *out, int cap) {
    uint32_t addr = 0;
    int kind;
    int n;
    if (net_ip_parse(text, &addr) != 0) {
        return -1;
    }
    kind = net_ip_class(addr);
    n = snprintf(out, (size_t)cap, "%c private=%d", kind ? kind : '?', net_ip_private(addr));
    return n > 0 && n < cap ? 0 : -1;
}

int net_cidr_line(const char *id, char *out, int cap) {
    if (id == NULL || out == NULL || cap <= 0) {
        return -1;
    }
    if (strcmp(id, "2010-37") == 0) {
        return line_2010(out, cap);
    }
    if (strcmp(id, "2009-47") == 0) {
        return line_2009(out, cap);
    }
    if (strcmp(id, "2018-38") == 0) {
        return line_2018(out, cap);
    }
    if (strcmp(id, "2019-37") == 0) {
        return line_2019(out, cap);
    }
    if (strcmp(id, "2021-35") == 0) {
        return line_2021(out, cap);
    }
    if (strcmp(id, "2012-39") == 0) {
        return line_2012(out, cap);
    }
    if (strcmp(id, "2011-38") == 0) {
        return line_2011_38(out, cap);
    }
    if (strcmp(id, "2011-37") == 0) {
        return line_pair("192.168.2.0/25", "192.168.2.128/25", out, cap) == 0
                   ? (int)strlen(out) + snprintf(out + strlen(out), (size_t)cap - strlen(out), " hop=192.168.1.2") < cap
                         ? 0
                         : -1
                   : -1;
    }
    if (strcmp(id, "2014-43") == 0) {
        return line_pair("192.1.6.0/24", "192.1.7.0/24", out, cap);
    }
    if (strcmp(id, "2018-47") == 0) {
        return line_2018_host(out, cap);
    }
    if (strcmp(id, "gap") == 0) {
        return line_pair("10.0.0.0/24", "10.0.2.0/24", out, cap);
    }
    if (strncmp(id, "class ", 6) == 0) {
        return line_class(id + 6, out, cap);
    }
    return -1;
}

static int frag_text(int total, int mtu, int df, char *out, int cap) {
    NetFrag piece[NET_FRAG_MAX];
    char off[64];
    char len[64];
    char mf[32];
    int n;
    int i;
    int used_off = 0;
    int used_len = 0;
    int used_mf = 0;
    n = net_ip_frag(total, 20, mtu, df, piece, NET_FRAG_MAX);
    if (n == -2) {
        snprintf(out, (size_t)cap, "drop DF=1");
        return 0;
    }
    if (n <= 0) {
        return -1;
    }
    off[0] = len[0] = mf[0] = '\0';
    for (i = 0; i < n; i++) {
        used_off += snprintf(off + used_off, sizeof off - (size_t)used_off, "%s%d", i ? "," : "", piece[i].offset);
        used_len += snprintf(len + used_len, sizeof len - (size_t)used_len, "%s%d", i ? "," : "", piece[i].total);
        used_mf += snprintf(mf + used_mf, sizeof mf - (size_t)used_mf, "%s%d", i ? "," : "", piece[i].mf);
    }
    n = snprintf(out, (size_t)cap, "n=%d max=%d off=%s len=%s mf=%s", n, piece[0].data, off, len, mf);
    return n > 0 && n < cap ? 0 : -1;
}

int net_frag_line(const char *arg, char *out, int cap) {
    int total = 0;
    int mtu = 0;
    if (arg == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(arg, "2018-47") == 0) {
        return frag_text(1500, 800, 0, out, cap);
    }
    if (strcmp(arg, "2021-36") == 0) {
        return frag_text(1580, 800, 0, out, cap);
    }
    if (sscanf(arg, "df %d %d", &total, &mtu) == 2) {
        return frag_text(total, mtu, 1, out, cap);
    }
    if (sscanf(arg, "%d %d", &total, &mtu) == 2) {
        return frag_text(total, mtu, 0, out, cap);
    }
    return -1;
}

static int join_ids(const int *id, int n, char *out, int cap) {
    int used = 0;
    int i;
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        int wrote = snprintf(out + used, (size_t)cap - (size_t)used, "%s%d", i ? "," : "", id[i]);
        if (wrote < 0 || used + wrote >= cap) {
            return -1;
        }
        used += wrote;
    }
    return 0;
}

int net_hdr_line(const char *id, char *out, int cap) {
    int sent[4];
    int hands[4];
    int pads[4];
    int ns = 0;
    int nh = 0;
    int np = 0;
    int app = 0;
    int routers = 0;
    char a[16];
    char b[16];
    char c[16];
    int n;
    if (id == NULL || strcmp(id, "2012-47") != 0) {
        return -1;
    }
    if (net_ip_2012(sent, &ns, hands, &nh, pads, &np, &app, &routers) != 0) {
        return -1;
    }
    if (join_ids(sent, ns, a, (int)sizeof a) != 0 || join_ids(hands, nh, b, (int)sizeof b) != 0 ||
        join_ids(pads, np, c, (int)sizeof c) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "sent=%s hands=%s pad=%s app=%d routers=%d", a, b, c, app, routers);
    return n > 0 && n < cap ? 0 : -1;
}
