#ifndef ENUMDFS_H
#define ENUMDFS_H

#include "graph.h"

/* 0 成功，-1 排列太多，-2 内存不足。失败时不打印。 */
int enum_dfs(const Graph *g, int start, FILE *out);

#endif
