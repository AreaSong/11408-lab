#ifndef WALK_H
#define WALK_H

#include "graph.h"

int walk_bfs(const Graph *g, int src, FILE *out);

int walk_bfs_all(const Graph *g, FILE *out);

int walk_dfs(const Graph *g, int src, FILE *out);

int walk_dfs_all(const Graph *g, FILE *out);

int walk_dfs_stack(const Graph *g, int src, FILE *out);

int walk_dfs_flip(const Graph *g, int src, FILE *out);

void walk_reach(const Graph *g, FILE *out);

/* 从每个未访问顶点启动一次。无向图里这个次数就是连通分量个数。 */
int adj_component_starts(int n, const int *len, const int adj[][G_MAX]);

/* 先从 start 做 DFS，走不到的顶点再按编号从小到大补开。order 的长度一定是 n。 */
void adj_dfs_cover(int n, const int *len, const int adj[][G_MAX], int start, int *order);

#endif
