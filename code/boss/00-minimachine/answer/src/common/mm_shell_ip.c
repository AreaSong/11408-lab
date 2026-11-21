#include "common/mm_shell_stage.h"

#include "net/net_arp.h"
#include "net/net_bgp.h"
#include "net/net_dhcp.h"
#include "net/net_fwd.h"
#include "net/net_ip.h"
#include "net/net_mcast.h"
#include "net/net_mip.h"
#include "net/net_nat.h"
#include "net/net_ospf.h"
#include "net/net_rip.h"
#include "net/net_sdn.h"
#include "net/net_stack.h"
#include "net/net_v6.h"

#include <stdio.h>
#include <string.h>

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int emit(FILE *out, const char *cmd, const char *body) {
    fprintf(out, "%s %s\n", cmd, body);
    return 1;
}

static int one_arg(const char *line, const char *word, int (*fn)(const char *, char *, int), FILE *out) {
    char buf[320];
    const char *arg = rest(line, word);
    if (arg[0] == '\0' || fn(arg, buf, (int)sizeof buf) != 0) {
        fprintf(out, "错误：%s 参数不对\n", word);
        return -1;
    }
    fprintf(out, "%s %s %s\n", word, arg, buf);
    return 1;
}

static int cmd_route(const char *arg, FILE *out) {
    char id[16];
    char dst[32];
    char buf[128];
    if (sscanf(arg, "%15s %31s", id, dst) != 2 || net_route_line(id, dst, buf, (int)sizeof buf) != 0) {
        fprintf(out, "错误：route 题号 目的地址\n");
        return -1;
    }
    fprintf(out, "route %s %s %s\n", id, dst, buf);
    return 1;
}

static int cmd_v6(const char *arg, FILE *out) {
    char op[16];
    char addr[64];
    char buf[80];
    int rc;
    if (sscanf(arg, "%15s %63s", op, addr) != 2) {
        fprintf(out, "错误：v6 compress 或 expand 地址\n");
        return -1;
    }
    rc = strcmp(op, "compress") == 0 ? net_v6_compress(addr, buf, (int)sizeof buf)
                                     : strcmp(op, "expand") == 0 ? net_v6_expand(addr, buf, (int)sizeof buf) : -1;
    if (rc != 0) {
        fprintf(out, "错误：v6 compress 或 expand 地址\n");
        return -1;
    }
    fprintf(out, "v6 %s\n", buf);
    return 1;
}

static int cmd_mcast(const char *arg, FILE *out) {
    char mac[24];
    int alias = 0;
    if (net_mcast_mac(arg, mac, (int)sizeof mac, &alias) != 0) {
        fprintf(out, "错误：mcast 组播地址\n");
        return -1;
    }
    fprintf(out, "mcast %s alias=%d\n", mac, alias);
    return 1;
}

static int cmd_mip(FILE *out) {
    unsigned cksum = 0;
    unsigned after = 0;
    unsigned colo = 0;
    unsigned colo_after = 0;
    if (net_mip_tunnel(&cksum, &after, &colo, &colo_after) != 0) {
        return -1;
    }
    fprintf(out, "mip coa=30.0.0.1 cksum=%04x hop=%04x colo=%04x colo2=%04x inner=10.0.0.8\n", cksum, after, colo,
            colo_after);
    return 1;
}

static int cmd_ip(char *line, FILE *out, MmShellOpt *opt);

static int cmd_calc(const char *line, FILE *out) {
    if (starts(line, "cidr")) {
        return one_arg(line, "cidr", net_cidr_line, out);
    }
    if (starts(line, "frag")) {
        return one_arg(line, "frag", net_frag_line, out);
    }
    if (starts(line, "hdr")) {
        return one_arg(line, "hdr", net_hdr_line, out);
    }
    if (starts(line, "route")) {
        return cmd_route(rest(line, "route"), out);
    }
    if (starts(line, "fwd")) {
        return one_arg(line, "fwd", net_fwd_line, out);
    }
    if (starts(line, "ttl")) {
        return one_arg(line, "ttl", net_fwd_line, out);
    }
    return 0;
}

static int cmd_proto(char *line, FILE *out, MmShellOpt *opt) {
    char buf[192];
    if (starts(line, "arp")) {
        return one_arg(line, "arp", net_arp_line, out);
    }
    if (starts(line, "nat")) {
        return one_arg(line, "nat", net_nat_line, out);
    }
    if (starts(line, "rip")) {
        return one_arg(line, "rip", net_rip_line, out);
    }
    if (starts(line, "ospf")) {
        return one_arg(line, "ospf", net_ospf_line, out);
    }
    if (strcmp(line, "dhcp") == 0) {
        return net_dhcp_line(buf, (int)sizeof buf) == 0 ? emit(out, "dhcp", buf) : -1;
    }
    if (starts(line, "bgp")) {
        return one_arg(line, "bgp", net_bgp_line, out);
    }
    if (starts(line, "sdn")) {
        return one_arg(line, "sdn", net_sdn_line, out);
    }
    if (starts(line, "v6")) {
        return cmd_v6(rest(line, "v6"), out);
    }
    if (starts(line, "mcast")) {
        return cmd_mcast(rest(line, "mcast"), out);
    }
    if (strcmp(line, "mip") == 0) {
        return cmd_mip(out);
    }
    if (starts(line, "nexam")) {
        const char *id = rest(line, "nexam");
        if (net_nexam(id, buf, (int)sizeof buf) != 0) {
            fprintf(out, "错误：nexam 题号\n");
            return -1;
        }
        fprintf(out, "nexam %s %s\n", id, buf);
        return 1;
    }
    return cmd_ip(line, out, opt);
}

static int cmd_ip(char *line, FILE *out, MmShellOpt *opt) {
    if (strcmp(line, "stack") == 0) {
        char body[640];
        if (!opt->ip_on) {
            fprintf(out, "错误：先执行 ip on\n");
            return -1;
        }
        if (net_stack_line(body, (int)sizeof body) != 0) {
            return -1;
        }
        fputs(body, out);
        return 1;
    }
    if (!starts(line, "ip")) {
        return 0;
    }
    {
        const char *arg = rest(line, "ip");
        if (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0) {
            fprintf(out, "错误：ip on 或 off\n");
            return -1;
        }
        opt->ip_on = strcmp(arg, "on") == 0;
        fprintf(out, "ip %s\n", arg);
        return 1;
    }
}

int mm_shell_ip(char *line, FILE *out, MmShellOpt *opt) {
    int calc = cmd_calc(line, out);
    if (calc != 0) {
        return calc;
    }
    return cmd_proto(line, out, opt);
}
