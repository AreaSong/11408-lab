#include "topo.h"

static int dfs_finish(const Graph *g, int u, int *color, int *rev, int *nr, FILE *trace) {
    color[u] = 1;
    if (trace != NULL) {
        fprintf(trace, "enter %d\n", u);
    }
    for (int v = 0; v < g->n; v++) {
        if (!graph_has(g, u, v)) {
            continue;
        }
        if (color[v] == 1) {
            return 1;
        }
        if (color[v] == 0 && dfs_finish(g, v, color, rev, nr, trace)) {
            return 1;
        }
    }
    color[u] = 2;
    rev[(*nr)++] = u;
    if (trace != NULL) {
        fprintf(trace, "finish %d\n", u);
    }
    return 0;
}

static void print_or_dash(FILE *trace, const char *name, const int *a, int n) {
    if (n == 0) {
        fprintf(trace, "%s -\n", name);
        return;
    }
    graph_row(trace, name, a, n, 0);
}

int topo_dfs(const Graph *g, int *topo, int *rev, int *n, int *cycle, FILE *trace) {
    int color[G_NMAX] = {0};
    int finish[G_NMAX];
    int nf = 0;
    int bad = 0;
    if (g->kind != G_D) {
        return G_KIND;
    }
    if (trace != NULL) {
        fputs("dfs-topo\n", trace);
    }
    for (int v = 0; v < g->n && !bad; v++) {
        if (color[v] == 0) {
            bad = dfs_finish(g, v, color, finish, &nf, trace);
        }
    }
    *cycle = bad;
    *n = bad ? 0 : nf;
    if (!bad) {
        for (int i = 0; i < nf; i++) {
            rev[i] = finish[i];
            topo[i] = finish[nf - 1 - i];
        }
    }
    if (trace != NULL) {
        fprintf(trace, "cycle %s\n", bad ? "yes" : "no");
        print_or_dash(trace, "rev", rev, *n);
        print_or_dash(trace, "topo", topo, *n);
    }
    return G_OK;
}
