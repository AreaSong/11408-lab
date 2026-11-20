#include "sdn.h"

#include "ls.h"

#include <stdio.h>
#include <string.h>

static int bad(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return -1;
}

void sdn_init(Sdn *s) {
    memset(s, 0, sizeof *s);
    topo_init(&s->topo);
}

int sdn_find_host(const Sdn *s, const char *name) {
    for (int i = 0; i < s->nh; i++) {
        if (strcmp(s->hname[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int sdn_add_switch(Sdn *s, const char *name, char *err, int cap) {
    if (sdn_find_host(s, name) >= 0) {
        return bad(err, cap, "名字和主机重复");
    }
    return topo_add_node(&s->topo, name, 0, err, cap);
}

int sdn_add_host(Sdn *s, const char *host, const char *sw, char *err, int cap) {
    int home;
    int n;

    if (strlen(host) == 0 || strlen(host) >= TOPO_NAME) {
        return bad(err, cap, "主机名是空的或太长");
    }
    if (sdn_find_host(s, host) >= 0 || topo_find(&s->topo, host) >= 0) {
        return bad(err, cap, "主机名重复");
    }
    home = topo_find(&s->topo, sw);
    if (home < 0) {
        return bad(err, cap, "主机要接在已经有的交换机上");
    }
    if (s->nh >= SDN_HMAX) {
        return bad(err, cap, "主机太多");
    }
    n = s->nh++;
    snprintf(s->hname[n], sizeof s->hname[n], "%s", host);
    s->home[n] = home;
    return n;
}

int sdn_link(Sdn *s, const char *a, const char *b, int cost, char *err, int cap) {
    int u;
    int v;
    int rc = topo_link(&s->topo, a, b, cost, err, cap);

    if (rc != 0) {
        return rc;
    }
    u = topo_find(&s->topo, a);
    v = topo_find(&s->topo, b);
    s->port[u][v] = ++s->nport[u];
    s->port[v][u] = ++s->nport[v];
    return 0;
}

int sdn_install(Sdn *s, char *err, int cap) {
    if (s->topo.n <= 0) {
        return bad(err, cap, "还没有交换机");
    }
    for (int sw = 0; sw < s->topo.n; sw++) {
        Sp sp;

        if (dijkstra_run(&s->topo.g, sw, &sp, NULL) != G_OK) {
            return bad(err, cap, "控制器没有算出路径");
        }
        for (int h = 0; h < s->nh; h++) {
            int dest = s->home[h];
            int nh;

            if (sw == dest) {
                s->flow[sw][h] = SDN_DELIVER;
                continue;
            }
            nh = ls_nexthop(&sp, dest);
            s->flow[sw][h] = nh < 0 ? SDN_DROP : nh;
        }
    }
    s->ready = 1;
    return 0;
}

int sdn_fail(Sdn *s, const char *a, const char *b, char *err, int cap) {
    if (topo_cut(&s->topo, a, b, err, cap) != 0) {
        return -1;
    }
    return sdn_install(s, err, cap);
}

int sdn_next(const Sdn *s, int sw, int host) {
    if (!s->ready || sw < 0 || host < 0 || sw >= s->topo.n || host >= s->nh) {
        return SDN_DROP;
    }
    return s->flow[sw][host];
}
