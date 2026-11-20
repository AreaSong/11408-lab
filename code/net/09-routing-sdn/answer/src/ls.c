#include "ls.h"

#include <stdio.h>
#include <string.h>

static int bad(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return -1;
}

int ls_nexthop(const Sp *sp, int dest) {
    int v = dest;
    int guard = 0;

    if (sp->dist[dest] >= G_INF) {
        return -1;
    }
    if (dest == sp->src) {
        return dest;
    }
    while (sp->pre[v] != sp->src) {
        v = sp->pre[v];
        if (v < 0 || ++guard > G_NMAX) {
            return -1;
        }
    }
    return v;
}

static int path_of(const Sp *sp, int dest, int *nodes, int cap) {
    int stack[G_NMAX];
    int n = 0;
    int x = dest;

    if (sp->dist[dest] >= G_INF) {
        return -1;
    }
    while (n < G_NMAX) {
        stack[n++] = x;
        if (x == sp->src) {
            break;
        }
        x = sp->pre[x];
        if (x < 0) {
            return -1;
        }
    }
    if (n > cap || stack[n - 1] != sp->src) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        nodes[i] = stack[n - 1 - i];
    }
    return n;
}

static void join_path(const Topo *t, const int *nodes, int n, char *buf, int cap) {
    int used = 0;

    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        const char *name = t->name[nodes[i]];
        int m = (int)strlen(name);
        int extra = i == 0 ? 0 : 1;

        if (used + extra + m + 1 > cap) {
            return;
        }
        if (i > 0) {
            buf[used++] = '>';
        }
        memcpy(buf + used, name, (size_t)m + 1);
        used += m;
    }
}

static void fill_row(const Topo *t, const Sp *sp, int dest, LsRow *row) {
    int nodes[G_NMAX];
    int npath;

    row->dest = dest;
    row->next = -1;
    row->cost = -1;
    row->direct_stub = 0;
    row->path[0] = '\0';
    npath = path_of(sp, dest, nodes, G_NMAX);
    if (npath < 2) {
        snprintf(row->path, sizeof row->path, "不可达");
        return;
    }
    row->cost = sp->dist[dest];
    /* 直连网络在评分表里写「直接到达」，不把路由器名再拼一遍。 */
    if (t->stub[dest] && npath == 2) {
        row->direct_stub = 1;
        snprintf(row->path, sizeof row->path, "直接到达");
        return;
    }
    row->next = nodes[1];
    join_path(t, nodes, npath, row->path, (int)sizeof row->path);
}

int ls_rows(const Topo *t, int src, int only_stub, LsRow *rows, int cap, int *nrow, char *err,
            int cap_err) {
    Sp sp;
    int n = 0;

    if (src < 0 || src >= t->n) {
        return bad(err, cap_err, "没有这个源");
    }
    if (dijkstra_run(&t->g, src, &sp, NULL) != G_OK) {
        return bad(err, cap_err, "Dijkstra 没有跑起来");
    }
    /* pick 不含源。子网按被确定的次序输出，和「费用不减」的步骤一致。 */
    for (int i = 0; i < sp.npick; i++) {
        int dest = sp.pick[i];

        if (only_stub && !t->stub[dest]) {
            continue;
        }
        if (n >= cap) {
            return bad(err, cap_err, "路由表放不下");
        }
        fill_row(t, &sp, dest, &rows[n]);
        n++;
    }
    *nrow = n;
    return 0;
}
