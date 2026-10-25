#include "show_topo.h"

#include "qty.h"

#include <stdio.h>
#include <string.h>

static void member_name(const Device *d, char *out, int cap) {
    if (d->kind == DEV_HOST && d->ip[0] != '\0') {
        snprintf(out, (size_t)cap, "%s（%s）", d->name, d->ip);
        return;
    }
    snprintf(out, (size_t)cap, "%s", d->name);
}

static void print_group(const Topo *t, const char *title, int index, const Group *g) {
    int i;

    printf("%s %d：", title, index);
    for (i = 0; i < g->n; i++) {
        char name[40];
        member_name(&t->dev[g->mem[i]], name, (int)sizeof name);
        printf("%s%s", i ? "、" : "", name);
    }
    printf("\n");
}

void show_domains(const Topo *t, const DomainReport *r) {
    int i;

    printf("【冲突域与广播域】\n");
    printf("冲突域：集线器连同它上面的主机和上联线算 1 个。\n");
    printf("交换机或路由器直连一台主机，这条链路算 1 个。\n");
    printf("交换机与交换机、交换机与路由器之间是全双工，两端同时发也碰不上，不算冲突域。\n");
    printf("广播域：广播过得了集线器，也过得了同一 VLAN 的交换机端口，过不了路由器。\n");
    for (i = 0; i < r->ncol; i++) {
        print_group(t, "冲突域", i + 1, &r->col[i]);
    }
    for (i = 0; i < r->nbrd; i++) {
        print_group(t, "广播域", i + 1, &r->brd[i]);
    }
    printf("结果：冲突域 %d 个，广播域 %d 个\n", r->ncol, r->nbrd);
}

void show_share(const Topo *t, const DomainReport *r, int a, int b) {
    int same_col = r->col_of[a] == r->col_of[b];
    int same_brd = r->brd_of[a] == r->brd_of[b];

    printf("【%s 与 %s】\n", t->dev[a].name, t->dev[b].name);
    printf("同一广播域：%s。\n", same_brd ? "是" : "否");
    printf("同一冲突域：%s。\n", same_col ? "是" : "否");
    printf("结果：%s广播域，%s冲突域\n", same_brd ? "同一" : "不同", same_col ? "同一" : "不同");
}

void show_table(const Device *d) {
    MacRow rows[MACTAB_CAP];
    int n = mactab_rows(&d->tab, rows, MACTAB_CAP);
    int i;

    printf("转发表 %s：\n", d->name);
    if (n == 0) {
        printf("（空）\n");
        return;
    }
    for (i = 0; i < n; i++) {
        char mac[MAC_TEXT];
        mac_fmt(rows[i].mac, mac);
        printf("%s  端口 %d  VLAN %d\n", mac, rows[i].port, rows[i].vlan);
    }
}

static int name_before(const Topo *t, int a, int b) {
    return strcmp(t->dev[a].name, t->dev[b].name) < 0;
}

static void sort_recv(const Topo *t, int *id, int n) {
    int i;

    for (i = 1; i < n; i++) {
        int key = id[i];
        int j = i;

        while (j > 0 && name_before(t, key, id[j - 1])) {
            id[j] = id[j - 1];
            j--;
        }
        id[j] = key;
    }
}

static void show_hops(const Topo *t, const FwdTrace *tr) {
    int i = 0;

    while (i < tr->nhop) {
        int sw = tr->hop_sw[i];

        int first = 1;

        printf("%s%s 转发端口", i ? "；" : "", t->dev[sw].name);
        while (i < tr->nhop && tr->hop_sw[i] == sw) {
            printf("%s%d", first ? " " : "、", tr->hop_port[i]);
            first = 0;
            i++;
        }
    }
}

static void show_result(const Topo *t, const FwdTrace *tr) {
    int id[FWD_RECV];
    int n = tr->nrecv;
    int i;

    memcpy(id, tr->recv, (size_t)n * sizeof id[0]);
    sort_recv(t, id, n);
    printf("结果：");
    if (tr->nhop > 0) {
        show_hops(t, tr);
        printf("；");
    } else if (tr->did_filter) {
        printf("%s 过滤；", t->dev[tr->filter_sw].name);
    }
    printf("收到 ");
    if (n == 0) {
        printf("无\n");
        return;
    }
    for (i = 0; i < n; i++) {
        char name[40];
        member_name(&t->dev[id[i]], name, (int)sizeof name);
        printf("%s%s", i ? " " : "", name);
    }
    printf("\n");
}

void show_fwd(const Topo *t, const FwdTrace *tr) {
    int i;

    for (i = 0; i < tr->nline; i++) {
        printf("%s\n", tr->line[i]);
    }
    for (i = 0; i < tr->nsw; i++) {
        show_table(&t->dev[tr->sw[i]]);
    }
    show_result(t, tr);
}

void show_compare(int n, const DomainReport *hub, const DomainReport *sw, const DomainReport *rt) {
    printf("【同一组主机】\n");
    printf("%d 台主机接到同一台设备上，分别换成集线器、交换机和路由器。\n", n);
    printf("集线器：冲突域 %d 个，广播域 %d 个。整台是一根总线，两个域都不隔离。\n", hub->ncol,
           hub->nbrd);
    printf("交换机：冲突域 %d 个，广播域 %d 个。每个端口一个冲突域，广播仍然全发。\n", sw->ncol,
           sw->nbrd);
    printf("路由器：冲突域 %d 个，广播域 %d 个。每个接口两边都隔开。\n", rt->ncol, rt->nbrd);
    printf("结果：集线器 %d %d，交换机 %d %d，路由器 %d %d\n", hub->ncol, hub->nbrd, sw->ncol,
           sw->nbrd, rt->ncol, rt->nbrd);
}

static void fmt_us(char *buf, int cap, long long ns) {
    long long ip = ns / 1000;
    long long frac = ns % 1000;

    if (frac == 0) {
        snprintf(buf, (size_t)cap, "%lld us", ip);
        return;
    }
    if (frac % 10 == 0) {
        snprintf(buf, (size_t)cap, "%lld.%02lld us", ip, frac / 10);
        return;
    }
    snprintf(buf, (size_t)cap, "%lld.%03lld us", ip, frac);
}

void show_delay(int store, long long frame_bytes, const LayTime *t, long long bps) {
    char rate[80];
    char us[40];
    long long bytes = store ? frame_bytes : 6;

    fmt_bps(rate, sizeof rate, bps);
    fmt_us(us, (int)sizeof us, t->ns);
    if (store) {
        printf("【存储转发】\n");
        printf("存储转发要收完整个帧才开始往外送。帧长含 FCS，不含前导码。\n");
    } else {
        printf("【直通交换】\n");
        printf("直通交换只看目的 MAC。目的 MAC 是 6 字节，到了就可以转发，不用等整帧。\n");
        printf("前导码不算。2013-38 的标准答案按这 6 字节算，不是按 14 字节首部。\n");
    }
    printf("%lld 字节 = %lld bit。\n", bytes, t->bits);
    printf("带宽 %s。\n", rate);
    printf("%lld bit / %lld bit/s = %lld ns = %s。\n", t->bits, bps, t->ns, us);
    printf("结果：%s\n", us);
}
