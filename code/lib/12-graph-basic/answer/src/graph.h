#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>

/* 顶点编号从 0 开始，和王道邻接矩阵的下标一致。简单图：无自环、无重边。
 * 无向边在矩阵里对称存两格，在邻接表里两个端点各挂一个边结点。
 * 邻接表用尾插，边结点次序等于输入次序。头插会把次序倒过来，
 * 同一张图的 BFS/DFS 序列就会变。adj 和链表是同一份次序，遍历走 adj。 */
#define G_MAX 16

typedef enum { G_NONE = 0, G_U = 1, G_D = 2 } GKind;

typedef struct ArcNode {
    int adjvex;
    struct ArcNode *nextarc;
} ArcNode;

typedef struct {
    GKind kind;
    int n;
    int e;
    int mat[G_MAX][G_MAX];
    int adj[G_MAX][G_MAX];
    int len[G_MAX];
    ArcNode *head[G_MAX];
    ArcNode *tail[G_MAX];
    int eu[G_MAX * G_MAX];
    int ev[G_MAX * G_MAX];
} Graph;

enum {
    ADD_OK = 0,
    ADD_LOOP = 1,
    ADD_DUP = 2,
    ADD_RANGE = 3,
    ADD_MEM = 4
};

/* g 必须全 0，或是上一次 graph_init 留下的图。 */
void graph_init(Graph *g, GKind kind, int n);

void graph_clear(Graph *g);

int graph_add(Graph *g, int u, int v);

/* 邻接矩阵不保留插入次序。按终点编号从小到大重排邻接表，无向边改成起点更小。 */
void graph_canon(Graph *g);

int graph_list_nodes(const Graph *g);

void graph_show(const Graph *g, FILE *out);

void graph_deg(const Graph *g, FILE *out);

void graph_space(const Graph *g, FILE *out);

#endif
