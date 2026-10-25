#include "topo.h"

#include "scan.h"

#include <stdio.h>
#include <string.h>

#define QMAX 64

static int peer(const Topo *t, int link, int dev) {
    if (t->link[link].a == dev) {
        return t->link[link].b;
    }
    return t->link[link].a;
}

static int push_mem(Group *g, int dev, char *err) {
    if (g->n >= DOM_MEM) {
        return lab_fail(err, "一个域里的设备太多了");
    }
    g->mem[g->n++] = dev;
    return 1;
}

static int take_hubs(Topo *t, DomainReport *r, int hub, int *seen, char *err) {
    int stack[TOPO_DEV];
    int n = 0;
    Group *g = &r->col[r->ncol];

    if (r->ncol >= DOM_MAX) {
        return lab_fail(err, "冲突域太多了");
    }
    stack[n++] = hub;
    seen[hub] = 1;
    if (!push_mem(g, hub, err)) {
        return 0;
    }
    while (n > 0) {
        int u = stack[--n];
        int i;

        for (i = 0; i < t->dev[u].nlink; i++) {
            int v = peer(t, t->dev[u].link[i], u);

            if (t->dev[v].kind == DEV_HUB && !seen[v]) {
                seen[v] = 1;
                stack[n++] = v;
                if (!push_mem(g, v, err)) {
                    return 0;
                }
            }
            if (t->dev[v].kind == DEV_HOST) {
                r->col_of[v] = r->ncol;
                if (!push_mem(g, v, err)) {
                    return 0;
                }
            }
        }
    }
    r->ncol++;
    return 1;
}

/* 集线器整片算一个。直连交换机或路由器的主机各算一个。全双工的设备间链路不算。 */
static int count_collision(Topo *t, DomainReport *r, char *err) {
    int seen[TOPO_DEV];
    int i;

    memset(seen, 0, sizeof seen);
    for (i = 0; i < t->ndev; i++) {
        if (t->dev[i].kind == DEV_HUB && !seen[i] && !take_hubs(t, r, i, seen, err)) {
            return 0;
        }
    }
    for (i = 0; i < t->ndev; i++) {
        if (t->dev[i].kind != DEV_HOST || r->col_of[i] >= 0) {
            continue;
        }
        if (t->dev[i].nlink == 0) {
            return lab_fail(err, "有主机还没接线");
        }
        if (r->ncol >= DOM_MAX || !push_mem(&r->col[r->ncol], i, err)) {
            return r->ncol >= DOM_MAX ? lab_fail(err, "冲突域太多了") : 0;
        }
        r->col_of[i] = r->ncol;
        r->ncol++;
    }
    return 1;
}

static int port_vlan(const Device *d, int link) {
    int i;

    for (i = 0; i < d->nlink; i++) {
        if (d->link[i] == link) {
            return d->pvlan[i];
        }
    }
    return 0;
}

typedef struct {
    const Topo *topo;
    DomainReport *rep;
    int qd[QMAX];
    int qv[QMAX];
    int ql[QMAX];
    int qn;
    int vd[QMAX];
    int vv[QMAX];
    int vn;
    char *err;
} Brd;

static int remember(Brd *b, int dev, int vlan) {
    int i;

    for (i = 0; i < b->vn; i++) {
        if (b->vd[i] == dev && b->vv[i] == vlan) {
            return 1;
        }
    }
    if (b->vn >= QMAX) {
        return -1;
    }
    b->vd[b->vn] = dev;
    b->vv[b->vn] = vlan;
    b->vn++;
    return 0;
}

static int enqueue(Brd *b, int dev, int vlan, int via) {
    if (b->qn >= QMAX) {
        return 0;
    }
    b->qd[b->qn] = dev;
    b->qv[b->qn] = vlan;
    b->ql[b->qn] = via;
    b->qn++;
    return 1;
}

static void claim(const Topo *t, DomainReport *r, int dev) {
    if (t->dev[dev].kind == DEV_HOST && r->brd_of[dev] < 0) {
        r->brd_of[dev] = r->nbrd;
    }
}

static int walk_links(Brd *b, int dev, int vlan, int via) {
    const Topo *t = b->topo;
    int i;
    int sw = t->dev[dev].kind == DEV_SWITCH;

    for (i = 0; i < t->dev[dev].nlink; i++) {
        int lk = t->dev[dev].link[i];
        int v;
        int next_vlan;
        int seen;

        if (lk == via || (sw && t->dev[dev].pvlan[i] != vlan)) {
            continue;
        }
        v = peer(t, lk, dev);
        if (t->dev[v].kind == DEV_ROUTER) {
            continue;
        }
        next_vlan = t->dev[v].kind == DEV_SWITCH ? port_vlan(&t->dev[v], lk) : 0;
        if (sw && t->dev[v].kind == DEV_SWITCH && next_vlan != vlan) {
            continue;
        }
        seen = remember(b, v, next_vlan);
        if (seen < 0 || (seen == 0 && !enqueue(b, v, next_vlan, lk))) {
            return lab_fail(b->err, "广播域的搜索范围太大");
        }
        if (seen == 0) {
            claim(t, b->rep, v);
        }
    }
    return 1;
}

static int one_broadcast(Topo *t, DomainReport *r, int start, char *err) {
    Brd b;
    int qh = 0;
    int i;

    memset(&b, 0, sizeof b);
    b.topo = t;
    b.rep = r;
    b.err = err;
    r->brd_of[start] = r->nbrd;
    if (remember(&b, start, 0) < 0 || !enqueue(&b, start, 0, -1)) {
        return lab_fail(err, "广播域的搜索范围太大");
    }
    while (qh < b.qn) {
        if (!walk_links(&b, b.qd[qh], b.qv[qh], b.ql[qh])) {
            return 0;
        }
        qh++;
    }
    if (r->nbrd >= DOM_MAX) {
        return lab_fail(err, "广播域太多了");
    }
    for (i = 0; i < t->ndev; i++) {
        if (r->brd_of[i] == r->nbrd && !push_mem(&r->brd[r->nbrd], i, err)) {
            return 0;
        }
    }
    r->nbrd++;
    return 1;
}

static int count_broadcast(Topo *t, DomainReport *r, char *err) {
    int i;

    for (i = 0; i < t->ndev; i++) {
        if (t->dev[i].kind == DEV_HOST && r->brd_of[i] < 0 && !one_broadcast(t, r, i, err)) {
            return 0;
        }
    }
    return 1;
}

static void sort_group(Group *g) {
    int i;

    for (i = 1; i < g->n; i++) {
        int key = g->mem[i];
        int j = i;

        while (j > 0 && g->mem[j - 1] > key) {
            g->mem[j] = g->mem[j - 1];
            j--;
        }
        g->mem[j] = key;
    }
}

int topo_domains(Topo *t, DomainReport *r, char *err) {
    int i;

    memset(r, 0, sizeof *r);
    for (i = 0; i < TOPO_DEV; i++) {
        r->col_of[i] = -1;
        r->brd_of[i] = -1;
    }
    if (!count_collision(t, r, err) || !count_broadcast(t, r, err)) {
        return 0;
    }
    for (i = 0; i < r->ncol; i++) {
        sort_group(&r->col[i]);
    }
    for (i = 0; i < r->nbrd; i++) {
        sort_group(&r->brd[i]);
    }
    return 1;
}

int topo_star(Topo *t, DevKind kind, int n, char *err) {
    int i;

    if (n < 2 || n > DEV_DEG) {
        return lab_fail(err, "主机数是 2～8");
    }
    topo_init(t);
    if (!topo_add(t, kind, "C", err)) {
        return 0;
    }
    for (i = 1; i <= n; i++) {
        unsigned char mac[MAC_LEN] = {0x02, 0, 0, 0, 0, 0};
        char name[8];

        mac[5] = (unsigned char)i;
        snprintf(name, sizeof name, "h%d", i);
        if (!topo_add_host(t, name, mac, NULL, err) || !topo_link(t, name, "C", -1, err)) {
            return 0;
        }
    }
    return 1;
}
