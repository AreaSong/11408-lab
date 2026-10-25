#ifndef RECOVER_H
#define RECOVER_H

#include "graph.h"

typedef struct {
    int nround;
    int avail0[BANK_N + 1][BANK_M];
    RedTrace trace[BANK_N + 1];
    int naborts;
    int abort_who[BANK_N];
    int abort_cost[BANK_N];
    int avail_after[BANK_N][BANK_M];
    int recovered;
    int no_holder;
} RecResult;

int recover_run(const Graph *src, const int *cost, RecResult *out);
void recover_print(const Graph *g, const RecResult *out);

#endif
