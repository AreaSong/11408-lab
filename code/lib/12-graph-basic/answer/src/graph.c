#include "graph.h"

#include <stdlib.h>
#include <string.h>

static void free_lists(Graph *g) {
    for (int i = 0; i < G_MAX; i++) {
        ArcNode *p = g->head[i];
        while (p != NULL) {
            ArcNode *next = p->nextarc;
            free(p);
            p = next;
        }
        g->head[i] = NULL;
        g->tail[i] = NULL;
        g->len[i] = 0;
    }
}

void graph_clear(Graph *g) {
    free_lists(g);
    memset(g, 0, sizeof *g);
}

void graph_init(Graph *g, GKind kind, int n) {
    graph_clear(g);
    g->kind = kind;
    g->n = n;
}

static int append_arc(Graph *g, int u, int v) {
    ArcNode *node;
    if (g->len[u] >= G_MAX) {
        return 0;
    }
    node = malloc(sizeof *node);
    if (node == NULL) {
        return 0;
    }
    node->adjvex = v;
    node->nextarc = NULL;
    if (g->tail[u] != NULL) {
        g->tail[u]->nextarc = node;
    } else {
        g->head[u] = node;
    }
    g->tail[u] = node;
    g->adj[u][g->len[u]++] = v;
    return 1;
}

int graph_add(Graph *g, int u, int v) {
    if (u < 0 || v < 0 || u >= g->n || v >= g->n) {
        return ADD_RANGE;
    }
    if (u == v) {
        return ADD_LOOP;
    }
    if (g->mat[u][v] != 0 || (g->kind == G_U && g->mat[v][u] != 0)) {
        return ADD_DUP;
    }
    if (g->e >= G_MAX * G_MAX) {
        return ADD_RANGE;
    }
    if (!append_arc(g, u, v)) {
        return ADD_MEM;
    }
    g->mat[u][v] = 1;
    if (g->kind == G_U && !append_arc(g, v, u)) {
        return ADD_MEM;
    }
    if (g->kind == G_U) {
        g->mat[v][u] = 1;
    }
    g->eu[g->e] = u;
    g->ev[g->e] = v;
    g->e++;
    return ADD_OK;
}

void graph_canon(Graph *g) {
    int ne = 0;
    free_lists(g);
    for (int u = 0; u < g->n; u++) {
        for (int v = 0; v < g->n; v++) {
            if (g->mat[u][v] == 0) {
                continue;
            }
            append_arc(g, u, v);
            /* 有向弧按起点、终点升序。无向边只记一次，起点取较小的编号。 */
            if (g->kind == G_D || u < v) {
                g->eu[ne] = u;
                g->ev[ne] = v;
                ne++;
            }
        }
    }
    g->e = ne;
}

int graph_list_nodes(const Graph *g) {
    int nodes = 0;
    for (int u = 0; u < g->n; u++) {
        nodes += g->len[u];
    }
    return nodes;
}

static void print_list(const Graph *g, FILE *out) {
    fprintf(out, "list\n");
    for (int u = 0; u < g->n; u++) {
        fprintf(out, "%d:", u);
        if (g->len[u] == 0) {
            fprintf(out, " -");
        }
        for (int i = 0; i < g->len[u]; i++) {
            fprintf(out, " %d", g->adj[u][i]);
        }
        fputc('\n', out);
    }
}

void graph_show(const Graph *g, FILE *out) {
    fprintf(out, "kind=%c n=%d e=%d\n", g->kind == G_U ? 'U' : 'D', g->n, g->e);
    fprintf(out, "mat\n");
    for (int u = 0; u < g->n; u++) {
        for (int v = 0; v < g->n; v++) {
            if (v > 0) {
                fputc(' ', out);
            }
            fprintf(out, "%d", g->mat[u][v]);
        }
        fputc('\n', out);
    }
    print_list(g, out);
}

static void deg_undirected(const Graph *g, FILE *out) {
    int sum = 0;
    for (int u = 0; u < g->n; u++) {
        fprintf(out, "%d=%d\n", u, g->len[u]);
        sum += g->len[u];
    }
    fprintf(out, "sum=%d\nedges=%d\ntwice=%d\nhandshake=%s\n", sum, g->e, 2 * g->e,
            sum == 2 * g->e ? "yes" : "no");
}

static void deg_directed(const Graph *g, FILE *out) {
    int in[G_MAX] = {0};
    int sum_out = 0;
    int sum_in = 0;
    for (int u = 0; u < g->n; u++) {
        for (int i = 0; i < g->len[u]; i++) {
            in[g->adj[u][i]]++;
        }
    }
    for (int u = 0; u < g->n; u++) {
        fprintf(out, "%d out=%d in=%d\n", u, g->len[u], in[u]);
        sum_out += g->len[u];
        sum_in += in[u];
    }
    fprintf(out, "sum-out=%d\nsum-in=%d\nedges=%d\nhandshake=%s\n", sum_out, sum_in, g->e,
            sum_out == g->e && sum_in == g->e ? "yes" : "no");
}

void graph_deg(const Graph *g, FILE *out) {
    fprintf(out, "deg\n");
    if (g->kind == G_U) {
        deg_undirected(g, out);
    } else {
        deg_directed(g, out);
    }
}

void graph_space(const Graph *g, FILE *out) {
    int list_edge = graph_list_nodes(g);
    fprintf(out, "n=%d\ne=%d\n", g->n, g->e);
    fprintf(out, "matrix-cells=%d\n", g->n * g->n);
    fprintf(out, "list-vertex=%d\nlist-edge=%d\nlist-nodes=%d\n", g->n, list_edge, g->n + list_edge);
    if (g->kind == G_D) {
        fprintf(out, "cross-nodes=%d\nmulti-nodes=skip\n", g->n + g->e);
    } else {
        fprintf(out, "cross-nodes=skip\nmulti-nodes=%d\n", g->n + g->e);
    }
}
