#ifndef PATH_H
#define PATH_H

#include "graph.h"

/*
 * Dijkstra 每轮从未确定顶点里选 dist 最小的（相等则编号小），再松弛。
 * hist[0] 是初始，hist[k] 是第 k 轮选完并松弛之后。
 * 真题问「求出第 k 条最短路径后」，读的是 hist[k]，不是全部跑完的 dist。
 * Floyd 允许负权边。某一轮让 dist[i][i] 变成负数，就是负环，结果不能再当最短路。
 */

typedef struct {
    int src;
    int dist[G_NMAX];
    int pre[G_NMAX];
    int pick[G_NMAX];
    int npick;
    int hist[G_NMAX][G_NMAX];
} Sp;

int dijkstra_run(const Graph *g, int src, Sp *out, FILE *trace);
int floyd_run(const Graph *g, int dist[G_NMAX][G_NMAX], int pre[G_NMAX][G_NMAX], int *negcycle,
              FILE *trace);

#endif
