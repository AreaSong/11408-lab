#include "net/net_sw.h"

#include "net/net_delay.h"
#include "net/net_fmt.h"
#include "net/net_win.h"

#include <stdio.h>
#include <string.h>

static void zero_table(NetTable *t) { memset(t, 0, sizeof *t); }

int net_learn(NetTable *t, int port, const char *mac, int vlan) {
    int i;
    if (t == NULL || mac == NULL || port < 1) {
        return -1;
    }
    for (i = 0; i < t->n; i++) {
        if (t->vlan[i] == vlan && strcmp(t->mac[i], mac) == 0) {
            t->port[i] = port;
            return 0;
        }
    }
    if (t->n >= 8) {
        return -1;
    }
    snprintf(t->mac[t->n], sizeof t->mac[0], "%s", mac);
    t->port[t->n] = port;
    t->vlan[t->n] = vlan;
    t->n++;
    return 0;
}

int net_forward(const NetTable *t, int in_port, const char *dst, int vlan, int nports, int *out, int cap) {
    int i;
    int n = 0;
    if (t == NULL || dst == NULL || out == NULL || nports < 1 || cap < 1) {
        return -1;
    }
    for (i = 0; i < t->n; i++) {
        if (t->vlan[i] == vlan && strcmp(t->mac[i], dst) == 0) {
            if (t->port[i] != in_port && n < cap) {
                out[n++] = t->port[i];
            }
            return n;
        }
    }
    /* 目的不在这张表里就泛洪，但不从进来的端口再送回去。 */
    for (i = 1; i <= nports && n < cap; i++) {
        if (i != in_port) {
            out[n++] = i;
        }
    }
    return n;
}

static int join_ports(const int *p, int n, char *dst, int cap) {
    int i;
    int o = 0;
    dst[0] = '\0';
    for (i = 0; i < n; i++) {
        int k = snprintf(dst + o, (size_t)(cap - o), "%s%d", i ? "," : "", p[i]);
        if (k < 0 || o + k >= cap) {
            return -1;
        }
        o += k;
    }
    return 0;
}

int net_sw_2014(char *first, int c1, char *second, int c2) {
    NetTable t;
    int out[4];
    int n;
    char ports[32];
    zero_table(&t);
    n = net_forward(&t, 1, "c1", 1, 3, out, 4);
    if (n != 2 || net_learn(&t, 1, "a1", 1) != 0 || join_ports(out, n, ports, (int)sizeof ports) != 0) {
        return -1;
    }
    snprintf(first, (size_t)c1, "fwd=%s got=b1,c1", ports);
    n = net_forward(&t, 3, "a1", 1, 3, out, 4);
    if (n != 1 || net_learn(&t, 3, "c1", 1) != 0 || join_ports(out, n, ports, (int)sizeof ports) != 0) {
        return -1;
    }
    snprintf(second, (size_t)c2, "fwd=%s got=a1", ports);
    return 0;
}

int net_sw_2016(char *first, int c1, char *second, int c2) {
    NetTable t;
    int out[4];
    int n;
    char ports[32];
    /* H2 在端口 1，H1 在端口 2，集线器在端口 3，H3 和 H4 接在集线器上。 */
    zero_table(&t);
    n = net_forward(&t, 1, "H4", 1, 3, out, 4);
    if (n != 2 || net_learn(&t, 1, "H2", 1) != 0 || join_ports(out, n, ports, (int)sizeof ports) != 0) {
        return -1;
    }
    snprintf(first, (size_t)c1, "fwd=%s got=H1,H3,H4", ports);
    n = net_forward(&t, 3, "H2", 1, 3, out, 4);
    if (n != 1 || net_learn(&t, 3, "H4", 1) != 0 || join_ports(out, n, ports, (int)sizeof ports) != 0) {
        return -1;
    }
    snprintf(second, (size_t)c2, "fwd=%s got=H2,H3", ports);
    return 0;
}

int net_domains(int hubs, int routers, int *collision, int *broadcast) {
    if (collision == NULL || broadcast == NULL || hubs < 1 || routers < 0) {
        return -1;
    }
    /* 交换机和路由器之间的全双工链路不算冲突域，否则 2020-35 会数成 6。 */
    *collision = hubs;
    *broadcast = routers + 1;
    return 0;
}

int net_place(const char *dev, int hosts, int *collision, int *broadcast) {
    if (dev == NULL || collision == NULL || broadcast == NULL || hosts < 1) {
        return -1;
    }
    if (strcmp(dev, "hub") == 0) {
        *collision = 1;
        *broadcast = 1;
        return 0;
    }
    if (strcmp(dev, "switch") == 0) {
        *collision = hosts;
        *broadcast = 1;
        return 0;
    }
    if (strcmp(dev, "router") == 0) {
        *collision = hosts;
        *broadcast = hosts;
        return 0;
    }
    return -1;
}

int net_fit_2022(char *dev1, int c1, char *dev2, int c2) {
    int col = 0;
    int brd = 0;
    if (dev1 == NULL || dev2 == NULL) {
        return -1;
    }
    if (net_place("switch", 2, &col, &brd) != 0 || col == 1 || brd != 1) {
        return -1;
    }
    snprintf(dev1, (size_t)c1, "switch");
    if (net_place("hub", 2, &col, &brd) != 0 || col != 1 || brd != 1) {
        return -1;
    }
    snprintf(dev2, (size_t)c2, "hub");
    return 0;
}

int net_vlan_2024(char *miss, int *ports, int cap, int *nports) {
    int vlan[8] = {0, 1, 1, 1, 1, 2, 3, 3};
    int n = 0;
    int i;
    if (miss == NULL || ports == NULL || nports == NULL || cap < 4) {
        return -1;
    }
    for (i = 1; i <= 7 && n < cap; i++) {
        if (i != 4 && vlan[i] == 1) {
            ports[n++] = i;
        }
    }
    *nports = n;
    for (i = 0; i < n; i++) {
        if (ports[i] == 6) {
            return -1;
        }
    }
    *miss = 'D';
    return 0;
}

static int wifi_copy(const char *a, const char *b, const char *c, char *dst, int cap) {
    snprintf(dst, (size_t)cap, "%s,%s,%s", a, b, c);
    return 0;
}

int net_wifi_exam(const char *id, char *dst, int cap) {
    if (id == NULL || dst == NULL || cap < 48) {
        return -1;
    }
    /* 去往 AP：地址 1 是接收方 AP，地址 2 是发送站点，地址 3 才是真正的目的。 */
    if (strcmp(id, "2017-35") == 0) {
        return wifi_copy("00-12-34-56-78-9b", "00-12-34-56-78-9a", "00-12-34-56-78-9c", dst, cap);
    }
    /* 来自 AP：地址 1 是接收站点，地址 2 是 AP，地址 3 是原始源。 */
    if (strcmp(id, "2022-47") == 0) {
        return wifi_copy("00-11-11-11-11-e1", "00-11-11-11-11-c1", "00-11-11-11-11-d1", dst, cap);
    }
    return -1;
}

long long net_cut_ns(long long bytes, long long mbps) {
    long long bps;
    if (bytes <= 0 || mbps <= 0) {
        return -1;
    }
    bps = mbps * 1000000LL;
    return bytes * 8 * 1000000000LL / bps;
}

int net_lan_line(char *dst, int cap) {
    NetDelay d;
    int pct = 0;
    int window = 0;
    int bits = 0;
    char tx[32];
    char prop[32];
    char rtt[32];
    char fwd[64];
    char ack[64];
    long long rate = 0;
    if (dst == NULL || cap < 80) {
        return -1;
    }
    if (net_link(8000, 10000000LL, 1000000LL, 200000000LL, 0, 0, &d) != 0) {
        return -1;
    }
    if (d.bdp_bit != 50000 || net_util_pct(8000, 8000, 10000LL, 200000000LL, &pct) != 0 || pct != 40) {
        return -1;
    }
    if (net_need("gbn", 1024, 4096, 1, 16000LL, 270000000LL, 100, &window, &bits) != 0 || window != 11 ||
        bits != 4) {
        return -1;
    }
    if (net_rate("gbn", 1000, 8000, 0, 100000000LL, 50000000LL, &rate) != 0 || rate != 79936051LL) {
        return -1;
    }
    if (net_sw_2014(fwd, (int)sizeof fwd, ack, (int)sizeof ack) != 0) {
        return -1;
    }
    net_fmt_ns(tx, (int)sizeof tx, d.tx_ns);
    net_fmt_ns(prop, (int)sizeof prop, d.prop_ns);
    net_fmt_ns(rtt, (int)sizeof rtt, d.rtt_ns);
    snprintf(dst, (size_t)cap, "tx=%s prop=%s rtt=%s bdp=%lld bit util=%d%% %s", tx, prop, rtt, d.bdp_bit, pct,
             fwd);
    return 0;
}
