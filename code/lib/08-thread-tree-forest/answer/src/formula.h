#ifndef FORMULA_H
#define FORMULA_H

#include "thr.h"

#include <stdio.h>

typedef struct {
    int deg;
    int count;
} DegItem;

/* 度的计数。缺的度当作 0。n0 = 1 + Σ(d-1)·nd，因为分支数 = n - 1。 */
int degree_print(const DegItem *items, int nitem, FILE *out);

/* 森林棵数 = 结点数 - 边数。每棵树少一条边。 */
int edges_print(int nodes, int edges, FILE *out);

/* 孩子兄弟表示里，没有右兄弟的结点 = 分支结点数 + 1（再加根）。 */
int noright_print(int nodes, int leaves, FILE *out);

/* 叶子高度为 1。k 叉树高度 h 最多 (k^h-1)/(k-1) 个结点。 */
int height_print(int ways, int nodes, FILE *out);

int k_ary_max(int ways, int height, long long *out);

#endif
