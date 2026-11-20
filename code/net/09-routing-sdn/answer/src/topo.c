#include "topo.h"

#include <stdio.h>
#include <string.h>

static int bad(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return -1;
}

static int name_ok(const char *name) {
    int n = (int)strlen(name);
    return n > 0 && n < TOPO_NAME;
}

void topo_init(Topo *t) {
    memset(t, 0, sizeof *t);
    graph_init(&t->g, G_U, 0);
}

int topo_find(const Topo *t, const char *name) {
    for (int i = 0; i < t->n; i++) {
        if (strcmp(t->name[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int topo_add_node(Topo *t, const char *name, int stub, char *err, int cap) {
    int id;

    if (!name_ok(name)) {
        return bad(err, cap, "结点名是空的或太长");
    }
    if (topo_find(t, name) >= 0) {
        return bad(err, cap, "结点名重复");
    }
    if (t->n >= G_NMAX) {
        return bad(err, cap, "结点太多");
    }
    id = t->n;
    t->n++;
    t->g.n = t->n;
    for (int i = 0; i < t->n; i++) {
        t->g.w[id][i] = i == id ? 0 : G_INF;
        t->g.w[i][id] = i == id ? 0 : G_INF;
    }
    snprintf(t->name[id], sizeof t->name[id], "%s", name);
    t->stub[id] = stub ? 1 : 0;
    return id;
}

int topo_link(Topo *t, const char *a, const char *b, int w, char *err, int cap) {
    int u = topo_find(t, a);
    int v = topo_find(t, b);
    int rc;

    if (u < 0 || v < 0) {
        return bad(err, cap, "链路两端要先建结点");
    }
    if (w <= 0) {
        return bad(err, cap, "链路代价要是正数，Dijkstra 不处理负权");
    }
    rc = graph_add(&t->g, u, v, w);
    if (rc == G_DUP) {
        return bad(err, cap, "这条链路已经有了");
    }
    if (rc != G_OK) {
        return bad(err, cap, "加不了这条链路");
    }
    return 0;
}

int topo_cut(Topo *t, const char *a, const char *b, char *err, int cap) {
    int u = topo_find(t, a);
    int v = topo_find(t, b);

    if (u < 0 || v < 0) {
        return bad(err, cap, "没有这台设备");
    }
    if (!graph_has(&t->g, u, v)) {
        return bad(err, cap, "这两台之间没有链路");
    }
    /* 两个方向都拆掉。第 13 关没有删边接口，空边就是 G_INF。 */
    t->g.w[u][v] = G_INF;
    t->g.w[v][u] = G_INF;
    return 0;
}
