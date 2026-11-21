#include "net/net_stp.h"

#include "ufset.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int a;
    int b;
    int cost;
} Edge;

static void sort_edges(Edge *e, int m) {
    int i;
    int j;
    for (i = 0; i < m; i++) {
        for (j = i + 1; j < m; j++) {
            if (e[j].cost < e[i].cost) {
                Edge t = e[i];
                e[i] = e[j];
                e[j] = t;
            }
        }
    }
}

static int sample(Edge *e) {
    e[0].a = 1;
    e[0].b = 2;
    e[0].cost = 1;
    e[1].a = 2;
    e[1].b = 3;
    e[1].cost = 2;
    e[2].a = 1;
    e[2].b = 3;
    e[2].cost = 4;
    return 3;
}

static int append_edge(char *dst, int cap, int *o, int a, int b, int first) {
    int n = snprintf(dst + *o, (size_t)(cap - *o), "%s%d-%d", first ? "" : ",", a, b);
    if (n < 0 || *o + n >= cap) {
        return -1;
    }
    *o += n;
    return 0;
}

static int kruskal(char *dst, int cap) {
    Edge e[4];
    Uf u;
    int m = sample(e);
    int o = 0;
    int i;
    int dropped = 0;
    sort_edges(e, m);
    uf_begin(&u, 3, UF_BOTH, NULL);
    snprintf(dst, (size_t)cap, "keep=");
    o = (int)strlen(dst);
    for (i = 0; i < m; i++) {
        if (uf_find(&u, e[i].a, NULL) == uf_find(&u, e[i].b, NULL)) {
            dropped = 1;
            continue;
        }
        if (uf_union(&u, e[i].a, e[i].b, NULL) != 0) {
            return -1;
        }
        if (append_edge(dst, cap, &o, e[i].a, e[i].b, o == 5) != 0) {
            return -1;
        }
    }
    if (!dropped) {
        return -1;
    }
    snprintf(dst + o, (size_t)(cap - o), " drop=1-3");
    return 0;
}

static int prim(char *dst, int cap) {
    Edge e[4];
    int in[4] = {0, 1, 0, 0};
    int used[4] = {0, 0, 0, 0};
    int m = sample(e);
    int grown = 0;
    int o;
    snprintf(dst, (size_t)cap, "order=");
    o = (int)strlen(dst);
    while (grown < 2) {
        int best = -1;
        int i;
        for (i = 0; i < m; i++) {
            int touch = in[e[i].a] + in[e[i].b];
            if (used[i] || touch != 1) {
                continue;
            }
            if (best < 0 || e[i].cost < e[best].cost) {
                best = i;
            }
        }
        if (best < 0 || append_edge(dst, cap, &o, e[best].a, e[best].b, grown == 0) != 0) {
            return -1;
        }
        in[e[best].a] = 1;
        in[e[best].b] = 1;
        used[best] = 1;
        grown++;
    }
    snprintf(dst + o, (size_t)(cap - o), " drop=1-3");
    return 0;
}

int net_stp_demo(const char *algo, char *dst, int cap) {
    if (algo == NULL || dst == NULL || cap < 32) {
        return -1;
    }
    if (strcmp(algo, "kruskal") == 0) {
        return kruskal(dst, cap);
    }
    if (strcmp(algo, "prim") == 0) {
        return prim(dst, cap);
    }
    return -1;
}

int net_flood_demo(char *dst, int cap) {
    int hop[5];
    int q[4];
    int head = 0;
    int tail = 0;
    int i;
    int edge_a[3] = {1, 2, 3};
    int edge_b[3] = {2, 3, 4};
    if (dst == NULL || cap < 16) {
        return -1;
    }
    for (i = 1; i <= 4; i++) {
        hop[i] = -1;
    }
    hop[1] = 0;
    q[tail++] = 1;
    while (head < tail) {
        int u = q[head++];
        for (i = 0; i < 3; i++) {
            int v = edge_a[i] == u ? edge_b[i] : (edge_b[i] == u ? edge_a[i] : 0);
            if (v != 0 && hop[v] < 0) {
                hop[v] = hop[u] + 1;
                q[tail++] = v;
            }
        }
    }
    snprintf(dst, (size_t)cap, "%d,%d,%d,%d", hop[1], hop[2], hop[3], hop[4]);
    return hop[4] == 3 ? 0 : -1;
}
