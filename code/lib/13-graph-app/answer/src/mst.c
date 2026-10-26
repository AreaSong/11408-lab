#include "mst.h"

#include "heap.h"
#include "ufset.h"

#include <stdio.h>

typedef struct {
    int u;
    int v;
    int w;
} Edge;

/* 权值可能为负，先平移成非负再编进堆的 key，弹出后才能拆回端点。 */
static int edge_key(int u, int v, int w) {
    int shifted = w + G_WMAX + 1;
    return shifted * (G_NMAX * G_NMAX) + u * G_NMAX + v;
}

static void key_decode(int key, Edge *e) {
    int code = key % (G_NMAX * G_NMAX);
    e->v = code % G_NMAX;
    e->u = code / G_NMAX;
    e->w = key / (G_NMAX * G_NMAX) - (G_WMAX + 1);
}

static void mst_clear(Mst *out) {
    out->cost = 0;
    out->span = 0;
    out->nedge = 0;
}

static void mst_add(Mst *out, int u, int v, int w) {
    if (u > v) {
        int tmp = u;
        u = v;
        v = tmp;
    }
    out->u[out->nedge] = u;
    out->v[out->nedge] = v;
    out->w[out->nedge] = w;
    out->nedge++;
    out->cost += w;
}

static void print_edges(const Mst *out, FILE *trace) {
    fputs("edges", trace);
    if (out->nedge == 0) {
        fputs(" -\n", trace);
        return;
    }
    for (int i = 0; i < out->nedge; i++) {
        fprintf(trace, " %d-%d", out->u[i], out->v[i]);
    }
    fputc('\n', trace);
}

static void print_low(const Graph *g, const int *low, const int *from, FILE *trace) {
    graph_row(trace, "low", low, g->n, 0);
    graph_row(trace, "from", from, g->n, 1);
}

static int pick_min(const Graph *g, const int *in, const int *low) {
    int best = G_INF;
    int pick = -1;
    for (int v = 0; v < g->n; v++) {
        if (!in[v] && low[v] < best) {
            best = low[v];
            pick = v;
        }
    }
    return pick;
}

static void print_alt(const Graph *g, const int *in, int pick, int via, int w, FILE *trace) {
    for (int u = 0; u < g->n; u++) {
        if (u == pick || u == via || !in[u]) {
            continue;
        }
        if (g->w[u][pick] == w) {
            fprintf(trace, " alt %d", u);
        }
    }
}

/* 严格小于才改 closest。相等的边留到选点时用 alt 报出来，两条都算「可能」。 */
static void relax_tree(const Graph *g, int pick, const int *in, int *low, int *from) {
    for (int v = 0; v < g->n; v++) {
        if (in[v] || !graph_has(g, pick, v)) {
            continue;
        }
        if (g->w[pick][v] < low[v]) {
            low[v] = g->w[pick][v];
            from[v] = pick;
        }
    }
}

static void prim_seed(const Graph *g, int start, int *in, int *low, int *from) {
    for (int v = 0; v < g->n; v++) {
        in[v] = 0;
        if (v == start) {
            low[v] = 0;
            from[v] = -1;
        } else if (graph_has(g, start, v)) {
            low[v] = g->w[start][v];
            from[v] = start;
        } else {
            low[v] = G_INF;
            from[v] = -1;
        }
    }
    in[start] = 1;
}

int prim_run(const Graph *g, int start, Mst *out, FILE *trace) {
    int in[G_NMAX];
    int low[G_NMAX];
    int from[G_NMAX];
    if (g->kind != G_U) {
        return G_KIND;
    }
    if (start < 0 || start >= g->n) {
        return G_RANGE;
    }
    mst_clear(out);
    prim_seed(g, start, in, low, from);
    if (trace != NULL) {
        fprintf(trace, "start %d\n", start);
        print_low(g, low, from, trace);
    }
    for (int round = 1; round < g->n; round++) {
        int pick = pick_min(g, in, low);
        if (pick < 0 || low[pick] >= G_INF) {
            if (trace != NULL) {
                fprintf(trace, "round %d stop\n", round);
            }
            break;
        }
        if (trace != NULL) {
            fprintf(trace, "round %d pick %d w=%d via %d", round, pick, low[pick], from[pick]);
            print_alt(g, in, pick, from[pick], low[pick], trace);
            fputc('\n', trace);
        }
        mst_add(out, from[pick], pick, low[pick]);
        in[pick] = 1;
        low[pick] = 0;
        relax_tree(g, pick, in, low, from);
        if (trace != NULL) {
            print_low(g, low, from, trace);
        }
    }
    out->span = out->nedge == g->n - 1;
    if (trace != NULL) {
        print_edges(out, trace);
        fprintf(trace, "cost %d\nspan %s\n", out->cost, out->span ? "yes" : "no");
    }
    return G_OK;
}

static int collect_edges(const Graph *g, Edge *e) {
    int m = 0;
    for (int u = 0; u < g->n; u++) {
        for (int v = u + 1; v < g->n; v++) {
            if (!graph_has(g, u, v)) {
                continue;
            }
            e[m].u = u;
            e[m].v = v;
            e[m].w = g->w[u][v];
            m++;
        }
    }
    return m;
}

static int heap_order(Edge *e, int m) {
    Heap h;
    heap_init(&h, 0);
    for (int i = 0; i < m; i++) {
        if (heap_push(&h, edge_key(e[i].u, e[i].v, e[i].w), NULL) != HEAP_OK) {
            return -1;
        }
    }
    for (int i = 0; i < m; i++) {
        int key = 0;
        if (heap_pop(&h, &key, NULL) != HEAP_OK) {
            return -1;
        }
        key_decode(key, &e[i]);
    }
    return 0;
}

static void print_sorted(const Edge *e, int m, FILE *trace) {
    fputs("sort", trace);
    if (m == 0) {
        fputs(" -\n", trace);
        return;
    }
    for (int i = 0; i < m; i++) {
        fprintf(trace, " %d-%d:%d", e[i].u, e[i].v, e[i].w);
    }
    fputc('\n', trace);
}

static int same_root(Uf *u, int a, int b) {
    return uf_find(u, a + 1, NULL) == uf_find(u, b + 1, NULL);
}

static void print_open(Uf *u, const Edge *e, int lo, int hi, FILE *trace) {
    int any = 0;
    fprintf(trace, "next %d open", e[lo].w);
    for (int k = lo; k < hi; k++) {
        if (!same_root(u, e[k].u, e[k].v)) {
            fprintf(trace, " %d-%d", e[k].u, e[k].v);
            any = 1;
        }
    }
    if (!any) {
        fputs(" -", trace);
    }
    fputc('\n', trace);
}

static void take_open(Uf *set, const Edge *edges, int lo, int hi, int need, Mst *out, FILE *trace) {
    if (trace != NULL) {
        print_open(set, edges, lo, hi, trace);
    }
    for (int k = lo; k < hi && out->nedge < need; k++) {
        if (same_root(set, edges[k].u, edges[k].v)) {
            if (trace != NULL) {
                fprintf(trace, "skip %d-%d\n", edges[k].u, edges[k].v);
            }
            continue;
        }
        uf_union(set, edges[k].u + 1, edges[k].v + 1, NULL);
        mst_add(out, edges[k].u, edges[k].v, edges[k].w);
        if (trace != NULL) {
            fprintf(trace, "add %d-%d\n", out->u[out->nedge - 1], out->v[out->nedge - 1]);
        }
    }
}

int kruskal_run(const Graph *g, Mst *out, FILE *trace) {
    Edge edges[G_NMAX * G_NMAX];
    Uf set;
    int m;
    int i = 0;
    if (g->kind != G_U) {
        return G_KIND;
    }
    m = collect_edges(g, edges);
    if (m > HEAP_MAX || (m > 0 && heap_order(edges, m) != 0)) {
        return G_HEAP;
    }
    mst_clear(out);
    uf_begin(&set, g->n, UF_RANK, NULL);
    if (trace != NULL) {
        fprintf(trace, "heap edges=%d\n", m);
        print_sorted(edges, m, trace);
    }
    while (i < m && out->nedge < g->n - 1) {
        int j = i + 1;
        while (j < m && edges[j].w == edges[i].w) {
            j++;
        }
        take_open(&set, edges, i, j, g->n - 1, out, trace);
        i = j;
    }
    out->span = g->n <= 1 || out->nedge == g->n - 1;
    if (trace != NULL) {
        print_edges(out, trace);
        fprintf(trace, "cost %d\nspan %s\n", out->cost, out->span ? "yes" : "no");
    }
    return G_OK;
}
