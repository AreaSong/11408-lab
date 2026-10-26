#ifndef CROSS_H
#define CROSS_H

#include "graph.h"

/* 有向图才建。无向图返回 -1，不打印。 */
int cross_print(const Graph *g, FILE *out);

#endif
