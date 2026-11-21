#include "net/net_nat.h"

#include "net/net_ip.h"

#include <stdio.h>
#include <string.h>

void net_nat_init(NetNat *box, uint32_t pub) {
    if (box == NULL) {
        return;
    }
    box->n = 0;
    box->pub = pub;
}

static int out_used(const NetNat *box, int port) {
    int i;
    for (i = 0; i < box->n; i++) {
        if (box->row[i].used && box->row[i].out_port == port) {
            return 1;
        }
    }
    return 0;
}

static int add_row(NetNat *box, uint32_t in_ip, int in_port, int out_port) {
    if (box->n >= NET_NAT_MAX || out_used(box, out_port)) {
        return -1;
    }
    box->row[box->n].in_ip = in_ip;
    box->row[box->n].in_port = in_port;
    box->row[box->n].out_ip = box->pub;
    box->row[box->n].out_port = out_port;
    box->row[box->n].used = 1;
    box->n++;
    return 0;
}

int net_nat_map(NetNat *box, uint32_t in_ip, int in_port, int *out_port) {
    int port;
    int i;
    if (box == NULL || out_port == NULL || in_port <= 0 || in_port > 65535) {
        return -1;
    }
    for (i = 0; i < box->n; i++) {
        if (box->row[i].used && box->row[i].in_ip == in_ip && box->row[i].in_port == in_port) {
            *out_port = box->row[i].out_port;
            return 0;
        }
    }
    port = in_port;
    while (out_used(box, port)) {
        if (port >= 65535) {
            return -1;
        }
        port++;
    }
    if (add_row(box, in_ip, in_port, port) != 0) {
        return -1;
    }
    *out_port = port;
    return 0;
}

int net_nat_static(NetNat *box, int out_port, uint32_t in_ip, int in_port) {
    return add_row(box, in_ip, in_port, out_port);
}

int net_nat_in(const NetNat *box, uint32_t dst, int dport, uint32_t *in_ip, int *in_port) {
    int i;
    if (box == NULL || in_ip == NULL || in_port == NULL || dst != box->pub) {
        return -1;
    }
    for (i = 0; i < box->n; i++) {
        if (box->row[i].used && box->row[i].out_port == dport) {
            *in_ip = box->row[i].in_ip;
            *in_port = box->row[i].in_port;
            return 0;
        }
    }
    return -1;
}

static int fmt_end(uint32_t ip, int port, char *out, int cap) {
    char text[16];
    int n;
    if (net_ip_fmt(ip, text, (int)sizeof text) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s:%d", text, port);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_2016(char *out, int cap) {
    NetNat box;
    uint32_t pub = 0;
    uint32_t src = 0;
    uint32_t dst = 0;
    int sport = 0;
    char a[24];
    char b[24];
    int n;
    if (net_ip_parse("201.1.3.10", &pub) != 0 || net_ip_parse("192.168.3.251", &src) != 0 ||
        net_ip_parse("130.18.10.1", &dst) != 0) {
        return -1;
    }
    net_nat_init(&box, pub);
    if (net_nat_map(&box, src, 49152, &sport) != 0 || fmt_end(pub, sport, a, (int)sizeof a) != 0 ||
        fmt_end(dst, 80, b, (int)sizeof b) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "src=%s dst=%s", a, b);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_2020(char *out, int cap) {
    NetNat r2;
    NetNat r3;
    uint32_t h2 = 0;
    uint32_t web_pub = 0;
    uint32_t web_in = 0;
    uint32_t r3_pub = 0;
    uint32_t back = 0;
    int back_port = 0;
    int sport = 0;
    char a[24];
    char b[24];
    char c[24];
    char d[24];
    int n;
    if (net_ip_parse("192.168.1.2", &h2) != 0 || net_ip_parse("203.10.2.2", &web_pub) != 0 ||
        net_ip_parse("203.10.2.6", &r3_pub) != 0) {
        return -1;
    }
    web_in = h2;
    net_nat_init(&r2, web_pub);
    net_nat_init(&r3, r3_pub);
    if (net_nat_static(&r2, 80, web_in, 80) != 0 || net_nat_map(&r3, h2, 50000, &sport) != 0 ||
        net_nat_in(&r2, web_pub, 80, &back, &back_port) != 0) {
        return -1;
    }
    if (fmt_end(h2, 50000, a, (int)sizeof a) != 0 || fmt_end(web_pub, 80, b, (int)sizeof b) != 0 ||
        fmt_end(r3_pub, sport, c, (int)sizeof c) != 0 || fmt_end(back, back_port, d, (int)sizeof d) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "h2=%s->%s r3=%s->%s r2=%s->%s", a, b, c, b, c, d);
    return n > 0 && n < cap ? 0 : -1;
}

int net_nat_line(const char *id, char *out, int cap) {
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2016-38") == 0) {
        return line_2016(out, cap);
    }
    if (strcmp(id, "2020-47") == 0) {
        return line_2020(out, cap);
    }
    return -1;
}
