#include "fwdlay.h"
#include "qty.h"
#include "scan.h"
#include "show_topo.h"

#include <stdio.h>
#include <string.h>

static Topo net;

static int die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int take(const char **p, char *buf, int cap) {
    int w = scan_word(p, buf, cap);

    if (w < 0) {
        die("这个词太长");
        return 0;
    }
    if (w == 0) {
        die("命令不完整");
        return 0;
    }
    return 1;
}

static int take_mac(const char **p, unsigned char mac[MAC_LEN]) {
    char w[24];
    char err[LAB_ERR];

    if (!take(p, w, (int)sizeof w)) {
        return 0;
    }
    if (!mac_parse(w, mac, err)) {
        die(err);
        return 0;
    }
    return 1;
}

static int cmd_host(const char *p) {
    char name[NAME_LEN];
    char ip[NAME_LEN];
    unsigned char mac[MAC_LEN];
    char err[LAB_ERR];
    int w;

    if (!take(&p, name, (int)sizeof name) || !take_mac(&p, mac)) {
        return 1;
    }
    ip[0] = '\0';
    w = scan_word(&p, ip, (int)sizeof ip);
    if (w < 0) {
        return die("这个词太长");
    }
    if (!scan_blank(p)) {
        return die("host 多了无法识别的内容");
    }
    if (!topo_add_host(&net, name, mac, w == 1 ? ip : NULL, err)) {
        return die(err);
    }
    return 0;
}

static int cmd_dev(const char *p) {
    char kind[16];
    char name[NAME_LEN];
    char err[LAB_ERR];
    DevKind k;

    if (!take(&p, kind, (int)sizeof kind) || !take(&p, name, (int)sizeof name)) {
        return 1;
    }
    if (!scan_blank(p)) {
        return die("dev 写成 dev hub|switch|router 名字");
    }
    if (strcmp(kind, "hub") == 0) {
        k = DEV_HUB;
    } else if (strcmp(kind, "switch") == 0) {
        k = DEV_SWITCH;
    } else if (strcmp(kind, "router") == 0) {
        k = DEV_ROUTER;
    } else {
        return die("设备只能是 hub、switch 或 router");
    }
    if (!topo_add(&net, k, name, err)) {
        return die(err);
    }
    return 0;
}

static int cmd_link(const char *p) {
    char a[NAME_LEN];
    char b[NAME_LEN];
    char w[16];
    char err[LAB_ERR];
    int vlan = -1;

    if (!take(&p, a, (int)sizeof a) || !take(&p, b, (int)sizeof b)) {
        return 1;
    }
    if (!scan_blank(p)) {
        int n = 0;
        if (!take(&p, w, (int)sizeof w)) {
            return 1;
        }
        if (strcmp(w, "vlan") != 0) {
            return die("链路末尾只能写 vlan 编号");
        }
        if (!take(&p, w, (int)sizeof w)) {
            return 1;
        }
        if (!scan_int(w, &n) || !scan_blank(p)) {
            return die("vlan 后面只跟编号");
        }
        vlan = n;
    }
    if (!topo_link(&net, a, b, vlan, err)) {
        return die(err);
    }
    return 0;
}

static int cmd_domains(const char *p) {
    DomainReport r;
    char err[LAB_ERR];

    if (!scan_blank(p)) {
        return die("domains 没有参数");
    }
    if (net.ndev == 0) {
        return die("还没有设备");
    }
    if (!topo_domains(&net, &r, err)) {
        return die(err);
    }
    show_domains(&net, &r);
    return 0;
}

static int cmd_share(const char *p) {
    char a[NAME_LEN];
    char b[NAME_LEN];
    DomainReport r;
    char err[LAB_ERR];
    int ia;
    int ib;

    if (!take(&p, a, (int)sizeof a) || !take(&p, b, (int)sizeof b)) {
        return 1;
    }
    if (!scan_blank(p)) {
        return die("share 写成 share 主机 主机");
    }
    ia = topo_find(&net, a);
    ib = topo_find(&net, b);
    if (ia < 0 || ib < 0 || net.dev[ia].kind != DEV_HOST || net.dev[ib].kind != DEV_HOST) {
        return die("share 的两端都要是主机");
    }
    if (ia == ib) {
        return die("请给两台不同的主机");
    }
    if (!topo_domains(&net, &r, err)) {
        return die(err);
    }
    show_share(&net, &r, ia, ib);
    return 0;
}

static int cmd_send(const char *p) {
    char name[NAME_LEN];
    unsigned char dst[MAC_LEN];
    FwdTrace tr;
    int host;

    if (!take(&p, name, (int)sizeof name) || !take_mac(&p, dst)) {
        return 1;
    }
    if (!scan_blank(p)) {
        return die("send 写成 send 主机 目的MAC");
    }
    host = topo_find(&net, name);
    if (!fwd_send(&net, host, dst, &tr)) {
        return die(tr.err);
    }
    show_fwd(&net, &tr);
    return 0;
}

static int cmd_table(const char *p) {
    char name[NAME_LEN];
    int id;

    if (!take(&p, name, (int)sizeof name)) {
        return 1;
    }
    if (!scan_blank(p)) {
        return die("table 写成 table 交换机");
    }
    id = topo_find(&net, name);
    if (id < 0 || net.dev[id].kind != DEV_SWITCH) {
        return die("table 只能看交换机");
    }
    show_table(&net.dev[id]);
    printf("结果：%d 条\n", net.dev[id].tab.count);
    return 0;
}

static int read_bw(const char **p, long long *bps) {
    char num[16];
    char unit[16];
    int v = 0;

    if (!take(p, num, (int)sizeof num)) {
        return 1;
    }
    if (strcmp(num, "bw") != 0) {
        return die("这里应该是 bw");
    }
    if (!take(p, num, (int)sizeof num) || !take(p, unit, (int)sizeof unit)) {
        return 1;
    }
    if (!scan_int(num, &v)) {
        return die("带宽要写成整数");
    }
    if (!parse_bps((double)v, unit, bps)) {
        return die("带宽单位用 bps、kbps、Mbps 或 Gbps");
    }
    return 0;
}

static int cmd_delay(const char *p) {
    char mode[16];
    long long bps = 0;
    long long frame = 6;
    LayTime lay;
    char err[LAB_ERR];
    int store;

    if (!take(&p, mode, (int)sizeof mode)) {
        return 1;
    }
    store = strcmp(mode, "store") == 0;
    if (!store && strcmp(mode, "cut") != 0) {
        return die("delay 后面是 cut 或 store");
    }
    if (read_bw(&p, &bps) != 0) {
        return 1;
    }
    if (store) {
        char w[16];
        int n = 0;
        if (!take(&p, w, (int)sizeof w)) {
            return 1;
        }
        if (strcmp(w, "frame") != 0) {
            return die("存储转发要写 frame 字节数 B");
        }
        if (!take(&p, w, (int)sizeof w)) {
            return 1;
        }
        if (!scan_int(w, &n)) {
            return die("帧长写成 frame 64 B");
        }
        if (!take(&p, w, (int)sizeof w) || strcmp(w, "B") != 0) {
            return die("帧长写成 frame 64 B");
        }
        frame = n;
    }
    if (!scan_blank(p)) {
        return die("delay 多了无法识别的内容");
    }
    if (!fwdlay_ns(store, bps, frame, &lay, err)) {
        return die(err);
    }
    show_delay(store, frame, &lay, bps);
    return 0;
}

static int cmd_compare(const char *p) {
    char w[16];
    int n = 0;
    Topo hub;
    Topo sw;
    Topo rt;
    DomainReport rh;
    DomainReport rs;
    DomainReport rr;
    char err[LAB_ERR];

    if (!take(&p, w, (int)sizeof w)) {
        return 1;
    }
    if (!scan_int(w, &n) || !scan_blank(p)) {
        return die("compare 后面跟主机数");
    }
    if (!topo_star(&hub, DEV_HUB, n, err) || !topo_domains(&hub, &rh, err) ||
        !topo_star(&sw, DEV_SWITCH, n, err) || !topo_domains(&sw, &rs, err) ||
        !topo_star(&rt, DEV_ROUTER, n, err) || !topo_domains(&rt, &rr, err)) {
        return die(err);
    }
    show_compare(n, &rh, &rs, &rr);
    return 0;
}

static int sim_command(const char *line) {
    char cmd[16];
    const char *p = line;
    int w = scan_word(&p, cmd, (int)sizeof cmd);

    if (w < 0) {
        return die("这个词太长");
    }
    if (strcmp(cmd, "host") == 0) {
        return cmd_host(p);
    }
    if (strcmp(cmd, "dev") == 0) {
        return cmd_dev(p);
    }
    if (strcmp(cmd, "link") == 0) {
        return cmd_link(p);
    }
    if (strcmp(cmd, "domains") == 0) {
        return cmd_domains(p);
    }
    if (strcmp(cmd, "share") == 0) {
        return cmd_share(p);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(p);
    }
    if (strcmp(cmd, "table") == 0) {
        return cmd_table(p);
    }
    if (strcmp(cmd, "delay") == 0) {
        return cmd_delay(p);
    }
    if (strcmp(cmd, "compare") == 0) {
        return cmd_compare(p);
    }
    if (strcmp(cmd, "clear") == 0) {
        if (!scan_blank(p)) {
            return die("clear 没有参数");
        }
        topo_init(&net);
        return 0;
    }
    return die("不认识的命令");
}

int main(void) {
    topo_init(&net);
    return lab_lines("请输入命令（host / dev / link / domains / share / send / table / delay / compare），Ctrl+D 结束：",
                     sim_command);
}
