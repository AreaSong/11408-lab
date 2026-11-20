#ifndef LS_H
#define LS_H

#include "topo.h"

#include "path.h"

/*
 * 链路状态先拼出全图，再对一台源跑第 13 关的 Dijkstra。
 * 路由表要的下一跳是离开源的第一台，不是最短路树上的父亲。
 */

#define LS_PATH 400

typedef struct {
    int dest;
    int next;
    int cost;
    int direct_stub;
    char path[LS_PATH];
} LsRow;

int ls_nexthop(const Sp *sp, int dest);
int ls_rows(const Topo *t, int src, int only_stub, LsRow *rows, int cap, int *nrow, char *err,
            int cap_err);

#endif
