#include "path.h"

#include <stdio.h>

static void copy_dist(const int *src, int *dst, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static void seed_dist(const Graph *g, int src, Sp *out) {
    out->src = src;
    out->npick = 0;
    for (int v = 0; v < g->n; v++) {
        if (v == src) {
            out->dist[v] = 0;
            out->pre[v] = -1;
        } else if (graph_has(g, src, v)) {
            out->dist[v] = g->w[src][v];
            out->pre[v] = src;
        } else {
            out->dist[v] = G_INF;
            out->pre[v] = -1;
        }
    }
    copy_dist(out->dist, out->hist[0], g->n);
}

static int pick_open(const Sp *out, const int *final, int n) {
    int pick = -1;
    for (int v = 0; v < n; v++) {
        if (final[v]) {
            continue;
        }
        if (pick < 0 || out->dist[v] < out->dist[pick]) {
            pick = v;
        }
    }
    return pick;
}

/* 已经确定的顶点不再改。负权边可能让后选的点给出更短的路，这时结果是错的。 */
static void relax_from(const Graph *g, Sp *out, const int *final, int pick) {
    if (out->dist[pick] >= G_INF) {
        return;
    }
    for (int v = 0; v < g->n; v++) {
        long long sum;
        if (final[v] || !graph_has(g, pick, v)) {
            continue;
        }
        sum = (long long)out->dist[pick] + g->w[pick][v];
        if (sum < out->dist[v]) {
            out->dist[v] = (int)sum;
            out->pre[v] = pick;
        }
    }
}

static void print_route(const Sp *out, int n, int v, FILE *trace) {
    int stack[G_NMAX];
    int top = 0;
    int x = v;
    if (out->dist[v] >= G_INF) {
        fprintf(trace, "route %d -\n", v);
        return;
    }
    while (top <= n) {
        stack[top++] = x;
        if (x == out->src) {
            break;
        }
        x = out->pre[x];
        if (x < 0) {
            break;
        }
    }
    fprintf(trace, "route %d ", v);
    for (int i = top - 1; i >= 0; i--) {
        if (i != top - 1) {
            fputc('>', trace);
        }
        fprintf(trace, "%d", stack[i]);
    }
    fputc('\n', trace);
}

static void trace_round(const Sp *out, int n, int round, int pick, FILE *trace) {
    fprintf(trace, "round %d pick %d dist=", round, pick);
    graph_cell(trace, out->hist[round - 1][pick]);
    fputc('\n', trace);
    graph_row(trace, "dist", out->dist, n, 0);
    graph_row(trace, "path", out->pre, n, 1);
}

int dijkstra_run(const Graph *g, int src, Sp *out, FILE *trace) {
    int final[G_NMAX] = {0};
    if (src < 0 || src >= g->n) {
        return G_RANGE;
    }
    seed_dist(g, src, out);
    final[src] = 1;
    if (trace != NULL) {
        if (g->neg > 0) {
            fputs("warn neg\n", trace);
        }
        fprintf(trace, "src %d\n", src);
        graph_row(trace, "dist", out->dist, g->n, 0);
        graph_row(trace, "path", out->pre, g->n, 1);
    }
    for (int round = 1; round < g->n; round++) {
        int pick = pick_open(out, final, g->n);
        final[pick] = 1;
        out->pick[out->npick++] = pick;
        relax_from(g, out, final, pick);
        copy_dist(out->dist, out->hist[round], g->n);
        if (trace != NULL) {
            trace_round(out, g->n, round, pick, trace);
        }
    }
    if (trace != NULL) {
        if (out->npick == 0) {
            fputs("order -\n", trace);
        } else {
            graph_row(trace, "order", out->pick, out->npick, 0);
        }
        for (int v = 0; v < g->n; v++) {
            print_route(out, g->n, v, trace);
        }
    }
    return G_OK;
}

static void floyd_seed(const Graph *g, int dist[G_NMAX][G_NMAX], int pre[G_NMAX][G_NMAX]) {
    for (int i = 0; i < g->n; i++) {
        for (int j = 0; j < g->n; j++) {
            dist[i][j] = g->w[i][j];
            if (i == j || g->w[i][j] >= G_INF) {
                pre[i][j] = -1;
            } else {
                pre[i][j] = i;
            }
        }
    }
}

static void floyd_pass(int n, int k, int dist[G_NMAX][G_NMAX], int pre[G_NMAX][G_NMAX]) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            long long sum;
            if (dist[i][k] >= G_INF || dist[k][j] >= G_INF) {
                continue;
            }
            sum = (long long)dist[i][k] + dist[k][j];
            /* 严格变短才改。相等时留下原来的前驱，和 Dijkstra 的「<」同一口径。 */
            if (sum >= dist[i][j]) {
                continue;
            }
            if (sum < -G_INF) {
                sum = -G_INF;
            }
            dist[i][j] = (int)sum;
            if (i != j) {
                pre[i][j] = pre[k][j];
            }
        }
    }
}

static int diagonal_neg(int n, int dist[G_NMAX][G_NMAX]) {
    for (int i = 0; i < n; i++) {
        if (dist[i][i] < 0) {
            return 1;
        }
    }
    return 0;
}

static void print_matrix(const char *title, int n, int dist[G_NMAX][G_NMAX], int neg_dash, FILE *trace) {
    fprintf(trace, "%s\n", title);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (j > 0) {
                fputc(' ', trace);
            }
            if (neg_dash && dist[i][j] < 0) {
                fputc('-', trace);
            } else {
                graph_cell(trace, dist[i][j]);
            }
        }
        fputc('\n', trace);
    }
}

int floyd_run(const Graph *g, int dist[G_NMAX][G_NMAX], int pre[G_NMAX][G_NMAX], int *negcycle,
              FILE *trace) {
    floyd_seed(g, dist, pre);
    if (trace != NULL) {
        print_matrix("k -", g->n, dist, 0, trace);
    }
    for (int k = 0; k < g->n; k++) {
        char title[16];
        floyd_pass(g->n, k, dist, pre);
        if (trace != NULL) {
            snprintf(title, sizeof title, "k %d", k);
            print_matrix(title, g->n, dist, 0, trace);
        }
    }
    *negcycle = diagonal_neg(g->n, dist);
    if (trace != NULL) {
        print_matrix("path", g->n, pre, 1, trace);
        fprintf(trace, "negcycle %s\n", *negcycle ? "yes" : "no");
    }
    return G_OK;
}
