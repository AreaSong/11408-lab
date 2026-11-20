#ifndef TOPO_H
#define TOPO_H

#include "graph.h"

/*
 * 路由器或子网都是图上的结点，名字在命令里写，算法只看下标。
 * 无向边：2014 年两边的 Metric 相同。OSPF 允许两个方向代价不同，那种题不能用这条边。
 * stub=1 的结点是要写进路由表的网络，不是中转路由器。
 */

#define TOPO_NAME 32

typedef struct {
    Graph g;
    int n;
    char name[G_NMAX][TOPO_NAME];
    int stub[G_NMAX];
} Topo;

void topo_init(Topo *t);
int topo_find(const Topo *t, const char *name);
int topo_add_node(Topo *t, const char *name, int stub, char *err, int cap);
int topo_link(Topo *t, const char *a, const char *b, int w, char *err, int cap);
int topo_cut(Topo *t, const char *a, const char *b, char *err, int cap);

#endif
