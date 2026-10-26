#include "topo.h"

#include <stdio.h>
#include <string.h>

static void indegree(const Graph *g, int *indeg) {
    for (int v = 0; v < g->n; v++) {
        indeg[v] = 0;
    }
    for (int u = 0; u < g->n; u++) {
        for (int v = 0; v < g->n; v++) {
            if (graph_has(g, u, v)) {
                indeg[v]++;
            }
        }
    }
}

static int ready_list(const Graph *g, const int *indeg, const int *used, int *ready) {
    int n = 0;
    for (int v = 0; v < g->n; v++) {
        if (!used[v] && indeg[v] == 0) {
            ready[n++] = v;
        }
    }
    return n;
}

static void drop_out(const Graph *g, int *indeg, int v) {
    for (int t = 0; t < g->n; t++) {
        if (graph_has(g, v, t)) {
            indeg[t]--;
        }
    }
}

static void print_left(const Graph *g, const int *used, FILE *trace) {
    int any = 0;
    fputs("left", trace);
    for (int v = 0; v < g->n; v++) {
        if (!used[v]) {
            fprintf(trace, " %d", v);
            any = 1;
        }
    }
    if (!any) {
        fputs(" -", trace);
    }
    fputc('\n', trace);
}

int topo_run(const Graph *g, TopoInfo *out, FILE *trace) {
    int indeg[G_NMAX];
    int used[G_NMAX] = {0};
    if (g->kind != G_D) {
        return G_KIND;
    }
    memset(out, 0, sizeof *out);
    out->unique = 1;
    indegree(g, indeg);
    for (int step = 0; step < g->n; step++) {
        int ready[G_NMAX];
        int nr = ready_list(g, indeg, used, ready);
        int v;
        if (nr == 0) {
            out->cycle = 1;
            out->unique = 0;
            break;
        }
        if (nr > 1) {
            out->unique = 0;
        }
        v = ready[0];
        if (trace != NULL) {
            fprintf(trace, "step %d pick %d ready", step, v);
            for (int i = 0; i < nr; i++) {
                fprintf(trace, " %d", ready[i]);
            }
            fputc('\n', trace);
        }
        used[v] = 1;
        out->order[out->norder++] = v;
        drop_out(g, indeg, v);
    }
    if (trace == NULL) {
        return G_OK;
    }
    if (out->norder == 0) {
        fputs("order -\n", trace);
    } else {
        graph_row(trace, "order", out->order, out->norder, 0);
    }
    fprintf(trace, "unique %s\ncycle %s\n", out->unique ? "yes" : "no", out->cycle ? "yes" : "no");
    if (out->cycle) {
        print_left(g, used, trace);
    }
    return G_OK;
}

typedef struct {
    const Graph *g;
    int indeg[G_NMAX];
    int used[G_NMAX];
    int seq[G_NMAX];
    int count;
    int stop;
    FILE *out;
} Walk;

static void walk(Walk *w, int depth) {
    if (w->stop) {
        return;
    }
    if (depth == w->g->n) {
        w->count++;
        if (w->count > G_TOPO_CAP) {
            w->stop = 1;
            return;
        }
        if (w->out != NULL) {
            graph_row(w->out, "seq", w->seq, depth, 0);
        }
        return;
    }
    for (int v = 0; v < w->g->n; v++) {
        if (w->used[v] || w->indeg[v] != 0) {
            continue;
        }
        w->used[v] = 1;
        w->seq[depth] = v;
        drop_out(w->g, w->indeg, v);
        walk(w, depth + 1);
        for (int t = 0; t < w->g->n; t++) {
            if (graph_has(w->g, v, t)) {
                w->indeg[t]++;
            }
        }
        w->used[v] = 0;
        if (w->stop) {
            return;
        }
    }
}

static void walk_reset(Walk *w, const Graph *g) {
    memset(w, 0, sizeof *w);
    w->g = g;
    indegree(g, w->indeg);
}

int topo_all(const Graph *g, int *count, FILE *trace) {
    Walk w;
    if (g->kind != G_D) {
        return G_KIND;
    }
    walk_reset(&w, g);
    walk(&w, 0);
    if (w.stop) {
        return G_MANY;
    }
    *count = w.count;
    if (trace == NULL) {
        return G_OK;
    }
    fprintf(trace, "count %d\n", *count);
    if (*count > 0) {
        walk_reset(&w, g);
        w.out = trace;
        walk(&w, 0);
    }
    fprintf(trace, "cycle %s\n", *count == 0 ? "yes" : "no");
    return G_OK;
}

int topo_check(const Graph *g, const int *seq, int n) {
    int seen[G_NMAX] = {0};
    int pos[G_NMAX];
    if (n != g->n) {
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (seq[i] < 0 || seq[i] >= n || seen[seq[i]]) {
            return 0;
        }
        seen[seq[i]] = 1;
        pos[seq[i]] = i;
    }
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (graph_has(g, u, v) && pos[u] > pos[v]) {
                return 0;
            }
        }
    }
    return 1;
}
