#include "net/net_sdn.h"

#include "graph.h"
#include "path.h"

#include <stdio.h>
#include <string.h>

static int first_hop(const Sp *sp, int dest) {
    int node = dest;
    int guard = 0;
    if (dest == sp->src || sp->dist[dest] >= G_INF) {
        return dest == sp->src ? dest : -1;
    }
    while (sp->pre[node] != sp->src) {
        node = sp->pre[node];
        if (node < 0 || ++guard > G_NMAX) {
            return -1;
        }
    }
    return node;
}

int net_sdn_hop(int broken, int *hop) {
    Graph graph;
    Sp sp;
    if (hop == NULL) {
        return -1;
    }
    graph_init(&graph, G_U, 3);
    if (graph_add(&graph, 0, 1, 1) != 0 || graph_add(&graph, 1, 2, 1) != 0) {
        return -1;
    }
    if (!broken && graph_add(&graph, 0, 2, 1) != 0) {
        return -1;
    }
    if (dijkstra_run(&graph, 0, &sp, NULL) != 0) {
        return -1;
    }
    *hop = first_hop(&sp, 2);
    return *hop < 0 ? -1 : 0;
}

int net_sdn_line(const char *id, char *out, int cap) {
    int before = 0;
    int after = 0;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2022-37") == 0) {
        n = snprintf(out, (size_t)cap, "南向 B");
        return n > 0 && n < cap ? 0 : -1;
    }
    if (strcmp(id, "fail") != 0 || net_sdn_hop(0, &before) != 0 || net_sdn_hop(1, &after) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "before=S%d after=S%d", before, after);
    return n > 0 && n < cap ? 0 : -1;
}
