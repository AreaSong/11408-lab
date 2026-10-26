#ifndef TOPO_H
#define TOPO_H

#include "graph.h"

/*
 * 拓扑排序用 Kahn：反复取入度为 0 的顶点。这里每次取编号最小的那个，序列才固定。
 * 某一步候选多于 1 个，拓扑序列就不唯一。顶点没排完，图里有环，序列不存在。
 * topo_all 按编号从小到大分支，输出的 seq 是字典序。超过 G_TOPO_CAP 条就停。
 */

typedef struct {
    int order[G_NMAX];
    int norder;
    int unique;
    int cycle;
} TopoInfo;

int topo_run(const Graph *g, TopoInfo *out, FILE *trace);
int topo_all(const Graph *g, int *count, FILE *trace);
int topo_check(const Graph *g, const int *seq, int n);

#endif
