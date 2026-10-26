#ifndef MULTI_H
#define MULTI_H

#include "graph.h"

/* 无向图才建。有向图返回 -1，不打印。 */
int multi_print(const Graph *g, FILE *out);

#endif
