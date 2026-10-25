#include "fwd.h"

#include "scan.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Topo *topo;
    FwdTrace *tr;
    const unsigned char *src;
    const unsigned char *dst;
    int origin;
    int depth;
} Walk;

static int say(FwdTrace *tr, const char *fmt, ...) {
    va_list ap;

    if (tr->nline >= FWD_LINE) {
        snprintf(tr->err, LAB_ERR, "转发过程太长");
        tr->bad = 1;
        return 0;
    }
    va_start(ap, fmt);
    vsnprintf(tr->line[tr->nline], FWD_TEXT, fmt, ap);
    va_end(ap);
    tr->nline++;
    return 1;
}

static int peer(const Topo *t, int link, int dev) {
    if (t->link[link].a == dev) {
        return t->link[link].b;
    }
    return t->link[link].a;
}

static int port_index(const Device *d, int link) {
    int i;

    for (i = 0; i < d->nlink; i++) {
        if (d->link[i] == link) {
            return i;
        }
    }
    return -1;
}

static void add_recv(FwdTrace *tr, int dev) {
    int i;

    for (i = 0; i < tr->nrecv; i++) {
        if (tr->recv[i] == dev) {
            return;
        }
    }
    if (tr->nrecv < FWD_RECV) {
        tr->recv[tr->nrecv++] = dev;
    }
}

static void add_hop(FwdTrace *tr, int sw, int port) {
    if (tr->nhop < FWD_HOP) {
        tr->hop_sw[tr->nhop] = sw;
        tr->hop_port[tr->nhop] = port;
        tr->nhop++;
    }
}

static void add_sw(FwdTrace *tr, int sw) {
    int i;

    for (i = 0; i < tr->nsw; i++) {
        if (tr->sw[i] == sw) {
            return;
        }
    }
    if (tr->nsw < FWD_SW) {
        tr->sw[tr->nsw++] = sw;
    }
}

static int learn_msg(FwdTrace *tr, MacTab *tab, const unsigned char *mac, int vlan, int port) {
    int old = 0;
    int how = mactab_learn(tab, mac, vlan, port, &old);

    if (how == 0) {
        snprintf(tr->err, LAB_ERR, "转发表满了");
        tr->bad = 1;
        return 0;
    }
    if (how == 1) {
        return say(tr, "学习：把源地址记到端口 %d。", port);
    }
    if (how == 2) {
        return say(tr, "学习：源地址已经在端口 %d。", port);
    }
    return say(tr, "学习：源地址从端口 %d 改到端口 %d。", old, port);
}

static int list_outs(const Device *d, int vlan, int in_link, int *outs) {
    int n = 0;
    int i;

    for (i = 0; i < d->nlink; i++) {
        if (d->link[i] == in_link || d->pvlan[i] != vlan) {
            continue;
        }
        outs[n++] = i;
    }
    return n;
}

static int pass(Walk *w, int from, int link);

static int emit_outs(Walk *w, int dev, const int *outs, int nout) {
    int i;
    Device *d = &w->topo->dev[dev];

    for (i = 0; i < nout; i++) {
        add_hop(w->tr, dev, outs[i] + 1);
        if (!pass(w, dev, d->link[outs[i]])) {
            return 0;
        }
    }
    return 1;
}

static int flood(Walk *w, int dev, int vlan, int in_link) {
    int outs[DEV_DEG];
    int nout = list_outs(&w->topo->dev[dev], vlan, in_link, outs);
    char buf[64];
    int used = 0;
    int i;
    const char *why = "目的不在这个 VLAN 的转发表里";

    if (mac_is_broadcast(w->dst)) {
        why = "目的是广播地址";
    } else if (mac_is_group(w->dst)) {
        why = "目的是组播地址";
    }
    if (nout == 0) {
        return say(w->tr, "%s，VLAN %d 里没有其他端口。", why, vlan);
    }
    buf[0] = '\0';
    for (i = 0; i < nout; i++) {
        used += snprintf(buf + used, sizeof buf - (size_t)used, "%s%d", i ? "、" : " ", outs[i] + 1);
    }
    if (!say(w->tr, "%s，向端口%s 泛洪。", why, buf)) {
        return 0;
    }
    return emit_outs(w, dev, outs, nout);
}

static int switch_on(Walk *w, int dev, int in_link) {
    Device *d = &w->topo->dev[dev];
    int idx = port_index(d, in_link);
    int port;
    int vlan;
    int dest_port = 0;
    int found;
    char sm[MAC_TEXT];
    char dm[MAC_TEXT];

    if (idx < 0) {
        snprintf(w->tr->err, LAB_ERR, "交换机找不到进入的端口");
        w->tr->bad = 1;
        return 0;
    }
    port = idx + 1;
    vlan = d->pvlan[idx];
    mac_fmt(w->src, sm);
    mac_fmt(w->dst, dm);
    add_sw(w->tr, dev);
    if (!say(w->tr, "交换机 %s 从端口 %d 收到，VLAN %d。源 %s，目的 %s。", d->name, port, vlan, sm,
             dm)) {
        return 0;
    }
    if (!learn_msg(w->tr, &d->tab, w->src, vlan, port)) {
        return 0;
    }
    found = !mac_is_group(w->dst) && mactab_find(&d->tab, w->dst, vlan, &dest_port);
    if (!found) {
        return flood(w, dev, vlan, in_link);
    }
    if (dest_port == port) {
        w->tr->did_filter = 1;
        w->tr->filter_sw = dev;
        return say(w->tr, "目的在端口 %d，和进来的端口相同，交换机丢掉这帧。", port);
    }
    add_hop(w->tr, dev, dest_port);
    if (!say(w->tr, "目的在端口 %d，只从该端口转发。", dest_port)) {
        return 0;
    }
    return pass(w, dev, d->link[dest_port - 1]);
}

static int hub_on(Walk *w, int dev, int in_link) {
    Device *d = &w->topo->dev[dev];
    int i;

    if (!say(w->tr, "集线器 %s 不看 MAC，把信号送到其余每个端口。", d->name)) {
        return 0;
    }
    for (i = 0; i < d->nlink; i++) {
        if (d->link[i] != in_link && !pass(w, dev, d->link[i])) {
            return 0;
        }
    }
    return 1;
}

static int receive(Walk *w, int dev, int in_link) {
    Device *d = &w->topo->dev[dev];

    if (d->kind == DEV_HOST) {
        if (dev == w->origin) {
            return say(w->tr, "帧回到了 %s，不再往下送。", d->name);
        }
        add_recv(w->tr, dev);
        return say(w->tr, "%s 收到。", d->name);
    }
    if (d->kind == DEV_HUB) {
        return hub_on(w, dev, in_link);
    }
    if (d->kind == DEV_ROUTER) {
        return say(w->tr, "路由器 %s 工作在网络层，不转发这个数据链路层帧。", d->name);
    }
    return switch_on(w, dev, in_link);
}

static int pass(Walk *w, int from, int link) {
    int rc;

    if (w->depth > 32) {
        snprintf(w->tr->err, LAB_ERR, "转发跳数太多，拓扑里可能有环");
        w->tr->bad = 1;
        return 0;
    }
    w->depth++;
    rc = receive(w, peer(w->topo, link, from), link);
    w->depth--;
    return rc;
}

int fwd_send(Topo *t, int host, const unsigned char dst[MAC_LEN], FwdTrace *tr) {
    Walk w;
    char dm[MAC_TEXT];
    Device *h;

    memset(tr, 0, sizeof *tr);
    if (host < 0 || host >= t->ndev || t->dev[host].kind != DEV_HOST) {
        return lab_fail(tr->err, "发送方必须是主机");
    }
    h = &t->dev[host];
    if (h->nlink != 1) {
        return lab_fail(tr->err, "这台主机还没接线");
    }
    if (mac_is_group(h->mac)) {
        return lab_fail(tr->err, "源 MAC 不能是组地址");
    }
    mac_fmt(dst, dm);
    w.topo = t;
    w.tr = tr;
    w.src = h->mac;
    w.dst = dst;
    w.origin = host;
    w.depth = 0;
    if (!say(tr, "【转发】") || !say(tr, "%s 发出，目的 %s。", h->name, dm) ||
        !say(tr, "转发决策看目的 MAC，自学习看源 MAC 和进入的端口。")) {
        return 0;
    }
    if (!pass(&w, host, h->link[0])) {
        if (!tr->bad) {
            snprintf(tr->err, LAB_ERR, "转发失败");
        }
        return 0;
    }
    return 1;
}
