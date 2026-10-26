#include "graph.h"

#include <stdio.h>

void graph_init(Graph *g, int kind, int n) {
    g->ready = 1;
    g->kind = kind;
    g->n = n;
    g->e = 0;
    g->neg = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            g->w[i][j] = i == j ? 0 : G_INF;
        }
    }
}

int graph_has(const Graph *g, int u, int v) {
    return u != v && g->w[u][v] < G_INF;
}

int graph_add(Graph *g, int u, int v, int w) {
    if (u < 0 || v < 0 || u >= g->n || v >= g->n || w < -G_WMAX || w > G_WMAX) {
        return G_RANGE;
    }
    if (u == v) {
        return G_LOOP;
    }
    if (graph_has(g, u, v)) {
        return G_DUP;
    }
    g->w[u][v] = w;
    if (g->kind == G_U) {
        g->w[v][u] = w;
    }
    g->e++;
    if (w < 0) {
        g->neg++;
    }
    return G_OK;
}

void graph_cell(FILE *out, int value) {
    /* 没有边印 inf，不印「-」。权值可以是负数，「-」和「-3」会黏在一起。 */
    if (value >= G_INF) {
        fputs("inf", out);
        return;
    }
    fprintf(out, "%d", value);
}

void graph_row(FILE *out, const char *name, const int *a, int n, int neg_dash) {
    fputs(name, out);
    for (int i = 0; i < n; i++) {
        fputc(' ', out);
        if (neg_dash && a[i] < 0) {
            fputc('-', out);
        } else {
            graph_cell(out, a[i]);
        }
    }
    fputc('\n', out);
}

void graph_show(const Graph *g, FILE *out) {
    fprintf(out, "kind=%c n=%d e=%d neg=%d\n", g->kind == G_U ? 'U' : 'D', g->n, g->e, g->neg);
    fputs("mat\n", out);
    for (int i = 0; i < g->n; i++) {
        for (int j = 0; j < g->n; j++) {
            if (j > 0) {
                fputc(' ', out);
            }
            graph_cell(out, g->w[i][j]);
        }
        fputc('\n', out);
    }
}
