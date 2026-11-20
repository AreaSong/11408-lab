#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>

/*
 * 带权简单图，邻接矩阵。顶点从 0 编号，和王道矩阵下标一致。
 * 对角线是 0。没有边的格子是 G_INF，打印成「-」。
 * 不用 INT_MAX：松弛时要做加法，INT_MAX 加正数会溢出。
 * 真题顶点若从 1 或字母起，调用前先平移到 0。
 */

#define G_NMAX 12
#define G_WMAX 1000000
#define G_INF 1000000000
#define G_TOPO_CAP 2000
#define G_PATH_CAP 32

enum { G_U = 0, G_D = 1 };

enum {
    G_OK = 0,
    G_RANGE = 1,
    G_LOOP = 2,
    G_DUP = 3,
    G_KIND = 4,
    G_HEAP = 5,
    G_NEG = 6,
    G_MANY = 7
};

typedef struct {
    int ready;
    int kind;
    int n;
    int e;
    int neg;
    int w[G_NMAX][G_NMAX];
} Graph;

void graph_init(Graph *g, int kind, int n);
int graph_add(Graph *g, int u, int v, int w);
int graph_has(const Graph *g, int u, int v);
void graph_cell(FILE *out, int value);
void graph_row(FILE *out, const char *name, const int *a, int n, int neg_dash);
void graph_show(const Graph *g, FILE *out);

#endif
