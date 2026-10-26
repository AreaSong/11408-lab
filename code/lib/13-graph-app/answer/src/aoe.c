#include "aoe.h"

#include "topo.h"

#include <stdio.h>
#include <string.h>

static void events(const Graph *g, const TopoInfo *topo, Aoe *out) {
    for (int v = 0; v < g->n; v++) {
        out->ve[v] = 0;
    }
    for (int i = 0; i < topo->norder; i++) {
        int u = topo->order[i];
        for (int v = 0; v < g->n; v++) {
            int reach;
            if (!graph_has(g, u, v)) {
                continue;
            }
            reach = out->ve[u] + g->w[u][v];
            if (reach > out->ve[v]) {
                out->ve[v] = reach;
            }
        }
    }
    out->len = 0;
    for (int v = 0; v < g->n; v++) {
        if (out->ve[v] > out->len) {
            out->len = out->ve[v];
        }
    }
    for (int v = 0; v < g->n; v++) {
        out->vl[v] = out->len;
    }
    for (int i = topo->norder - 1; i >= 0; i--) {
        int u = topo->order[i];
        for (int v = 0; v < g->n; v++) {
            int allow;
            if (!graph_has(g, u, v)) {
                continue;
            }
            allow = out->vl[v] - g->w[u][v];
            if (allow < out->vl[u]) {
                out->vl[u] = allow;
            }
        }
    }
}

static void fill_acts(const Graph *g, Aoe *out) {
    out->nact = 0;
    for (int u = 0; u < g->n; u++) {
        for (int v = 0; v < g->n; v++) {
            Act *a;
            if (!graph_has(g, u, v)) {
                continue;
            }
            a = &out->act[out->nact++];
            a->u = u;
            a->v = v;
            a->w = g->w[u][v];
            a->early = out->ve[u];
            a->late = out->vl[v] - g->w[u][v];
            a->slack = a->late - a->early;
            a->key = a->slack == 0;
        }
    }
}

static int key_next(const Aoe *out, int u, int *vs) {
    int n = 0;
    for (int i = 0; i < out->nact; i++) {
        if (out->act[i].key && out->act[i].u == u) {
            vs[n++] = out->act[i].v;
        }
    }
    return n;
}

static void save_path(Aoe *out, const int *cur, int depth) {
    if (out->npath >= G_PATH_CAP) {
        out->overflow = 1;
        return;
    }
    for (int i = 0; i < depth; i++) {
        out->path[out->npath][i] = cur[i];
    }
    out->plen[out->npath] = depth;
    out->npath++;
}

static void dfs_key(Aoe *out, int u, int *cur, int depth) {
    int next[G_NMAX];
    int nnext;
    cur[depth] = u;
    nnext = key_next(out, u, next);
    if (nnext == 0) {
        if (out->ve[u] == out->len) {
            save_path(out, cur, depth + 1);
        }
        return;
    }
    for (int i = 0; i < nnext; i++) {
        dfs_key(out, next[i], cur, depth + 1);
    }
}

static void from_sources(const Graph *g, Aoe *out) {
    int indeg[G_NMAX] = {0};
    int cur[G_NMAX];
    for (int i = 0; i < out->nact; i++) {
        indeg[out->act[i].v]++;
    }
    for (int u = 0; u < g->n; u++) {
        if (indeg[u] == 0) {
            dfs_key(out, u, cur, 0);
        }
    }
}

static int cmp_path(const Aoe *out, int i, int j) {
    int n = out->plen[i] < out->plen[j] ? out->plen[i] : out->plen[j];
    for (int k = 0; k < n; k++) {
        if (out->path[i][k] != out->path[j][k]) {
            return out->path[i][k] - out->path[j][k];
        }
    }
    return out->plen[i] - out->plen[j];
}

static void swap_path(Aoe *out, int i, int j) {
    int tmp[G_NMAX];
    int ni = out->plen[i];
    int nj = out->plen[j];
    memcpy(tmp, out->path[i], (size_t)ni * sizeof tmp[0]);
    memcpy(out->path[i], out->path[j], (size_t)nj * sizeof tmp[0]);
    memcpy(out->path[j], tmp, (size_t)ni * sizeof tmp[0]);
    out->plen[i] = nj;
    out->plen[j] = ni;
}

static void sort_paths(Aoe *out) {
    for (int i = 1; i < out->npath; i++) {
        int j = i;
        while (j > 0 && cmp_path(out, j, j - 1) < 0) {
            swap_path(out, j, j - 1);
            j--;
        }
    }
}

static int edge_on(const Aoe *out, int p, int u, int v) {
    for (int i = 0; i + 1 < out->plen[p]; i++) {
        if (out->path[p][i] == u && out->path[p][i + 1] == v) {
            return 1;
        }
    }
    return 0;
}

/* 落在每一条关键路径上的活动，单独缩短才会让工期下降。 */
static void mark_alone(Aoe *out) {
    out->nalone = 0;
    if (out->overflow || out->npath == 0) {
        return;
    }
    for (int i = 0; i < out->nact; i++) {
        int hit = 0;
        if (!out->act[i].key) {
            continue;
        }
        for (int p = 0; p < out->npath; p++) {
            hit += edge_on(out, p, out->act[i].u, out->act[i].v);
        }
        if (hit == out->npath) {
            out->alone_u[out->nalone] = out->act[i].u;
            out->alone_v[out->nalone] = out->act[i].v;
            out->nalone++;
        }
    }
}

static void print_path(const Aoe *out, int p, FILE *trace) {
    fputs("path ", trace);
    for (int i = 0; i < out->plen[p]; i++) {
        if (i > 0) {
            fputc('>', trace);
        }
        fprintf(trace, "%d", out->path[p][i]);
    }
    fputc('\n', trace);
}

static void print_result(const Graph *g, const Aoe *out, FILE *trace) {
    graph_row(trace, "ve", out->ve, g->n, 0);
    graph_row(trace, "vl", out->vl, g->n, 0);
    fprintf(trace, "len %d\n", out->len);
    for (int i = 0; i < out->nact; i++) {
        const Act *a = &out->act[i];
        fprintf(trace, "act %d>%d w=%d e=%d l=%d slack=%d%s\n", a->u, a->v, a->w, a->early, a->late,
                a->slack, a->key ? " key" : "");
    }
    fprintf(trace, "paths %d\n", out->npath);
    for (int p = 0; p < out->npath; p++) {
        print_path(out, p, trace);
    }
    fputs("alone", trace);
    if (out->overflow) {
        fputs(" fail\n", trace);
        return;
    }
    if (out->nalone == 0) {
        fputs(" -\n", trace);
        return;
    }
    for (int i = 0; i < out->nalone; i++) {
        fprintf(trace, " %d>%d", out->alone_u[i], out->alone_v[i]);
    }
    fputc('\n', trace);
}

int aoe_run(const Graph *g, Aoe *out, FILE *trace) {
    TopoInfo topo;
    int rc;
    if (g->kind != G_D) {
        return G_KIND;
    }
    if (g->neg > 0) {
        return G_NEG;
    }
    memset(out, 0, sizeof *out);
    rc = topo_run(g, &topo, NULL);
    if (rc != G_OK) {
        return rc;
    }
    if (topo.cycle) {
        out->cycle = 1;
        if (trace != NULL) {
            fputs("cycle yes\n", trace);
        }
        return G_OK;
    }
    events(g, &topo, out);
    fill_acts(g, out);
    from_sources(g, out);
    sort_paths(out);
    mark_alone(out);
    if (trace != NULL) {
        print_result(g, out, trace);
    }
    return G_OK;
}
