#ifndef GRAPH_H
#define GRAPH_H

#include "num.h"

typedef struct {
    int n;
    int m;
    int base;
    int avail[BANK_M];
    int alloc[BANK_N][BANK_M];
    int req[BANK_N][BANK_M];
} Graph;

typedef struct {
    int nstep;
    TraceStep steps[BANK_N];
    int deadlock;
    int removed[BANK_N];
    int nremoved;
    int left[BANK_N];
    int nleft;
    int isolated[BANK_N];
    int niso;
    int stuck[BANK_M];
} RedTrace;

int graph_reduce(const Graph *g, RedTrace *out);
void graph_print(const Graph *g, const RedTrace *t);

#endif
