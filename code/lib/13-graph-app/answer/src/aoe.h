#ifndef AOE_H
#define AOE_H

#include "graph.h"

/*
 * AOE 网：顶点是事件，边是活动，权是持续时间，必须非负。
 * ve 沿拓扑正序取最大，vl 沿逆序取最小。活动的 e = ve[起点]，l = vl[终点] - 权。
 * slack = l - e。slack 为 0 的活动在某条关键路径上。
 * alone 是每条关键路径都经过的活动：只缩短它，工期才会下降。
 */

typedef struct {
    int u;
    int v;
    int w;
    int early;
    int late;
    int slack;
    int key;
} Act;

typedef struct {
    int ve[G_NMAX];
    int vl[G_NMAX];
    int len;
    int cycle;
    int nact;
    Act act[G_NMAX * G_NMAX];
    int npath;
    int path[G_PATH_CAP][G_NMAX];
    int plen[G_PATH_CAP];
    int nalone;
    int alone_u[G_NMAX * G_NMAX];
    int alone_v[G_NMAX * G_NMAX];
    int overflow;
} Aoe;

int aoe_run(const Graph *g, Aoe *out, FILE *trace);

#endif
