#include "net/net_ospf.h"

#include "graph.h"
#include "path.h"

#include <stdio.h>
#include <string.h>

static int first_hop(const Sp *sp, int dest) {
    int node = dest;
    int guard = 0;
    if (sp->dist[dest] >= G_INF) {
        return -1;
    }
    if (dest == sp->src) {
        return dest;
    }
    while (sp->pre[node] != sp->src) {
        node = sp->pre[node];
        if (node < 0 || ++guard > G_NMAX) {
            return -1;
        }
    }
    return node;
}

static int build_2014(Graph *graph) {
    graph_init(graph, G_U, 8);
    if (graph_add(graph, 0, 4, 1) != 0 || graph_add(graph, 0, 2, 2) != 0 || graph_add(graph, 2, 5, 1) != 0) {
        return -1;
    }
    if (graph_add(graph, 0, 1, 3) != 0 || graph_add(graph, 1, 6, 1) != 0 || graph_add(graph, 1, 3, 4) != 0) {
        return -1;
    }
    return graph_add(graph, 3, 7, 1) == 0 && graph_add(graph, 2, 3, 6) == 0 ? 0 : -1;
}

int net_ospf_2014(char hop[4][8], int cost[4]) {
    static const int subnet[] = {4, 5, 6, 7};
    static const char *name[] = {"R1", "R2", "R3", "R4"};
    Graph graph;
    Sp sp;
    int i;
    if (hop == NULL || cost == NULL || build_2014(&graph) != 0 || dijkstra_run(&graph, 0, &sp, NULL) != 0) {
        return -1;
    }
    for (i = 0; i < 4; i++) {
        int next = first_hop(&sp, subnet[i]);
        if (next < 0) {
            return -1;
        }
        cost[i] = sp.dist[subnet[i]];
        if (next >= 4) {
            snprintf(hop[i], 8, "直连");
        } else {
            snprintf(hop[i], 8, "%s", name[next]);
        }
    }
    return 0;
}

int net_ospf_line(const char *id, char *out, int cap) {
    static const char *net[] = {"192.1.1.0/24", "192.1.5.0/24", "192.1.6.0/24", "192.1.7.0/24"};
    char hop[4][8];
    int cost[4];
    int n;
    if (id == NULL || strcmp(id, "2014-43") != 0 || net_ospf_2014(hop, cost) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s %s %d; %s %s %d; %s %s %d; %s %s %d; lsi=0.0.0.0/0 metric=10", net[0],
                 hop[0], cost[0], net[1], hop[1], cost[1], net[2], hop[2], cost[2], net[3], hop[3], cost[3]);
    return n > 0 && n < cap ? 0 : -1;
}
