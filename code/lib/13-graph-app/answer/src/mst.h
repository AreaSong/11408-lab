#ifndef MST_H
#define MST_H

#include "graph.h"

/*
 * Prim 是教科书的 O(n^2) 扫描，不用堆。选择题要模拟的是 lowcost，不是堆优化版。
 * 权值相等时严格小于才改 closest，和王道「<」一致；并列的另一条边打在 alt 上。
 * Kruskal 把边推进第 09 关的小根堆，弹出的次序就是权值、再按端点编号。
 * 成环与否问并查集：图顶点 0..n-1，并查集编号是 1..n。
 */

typedef struct {
    int cost;
    int span;
    int nedge;
    int u[G_NMAX];
    int v[G_NMAX];
    int w[G_NMAX];
} Mst;

int prim_run(const Graph *g, int start, Mst *out, FILE *trace);
int kruskal_run(const Graph *g, Mst *out, FILE *trace);

#endif
