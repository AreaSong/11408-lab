#ifndef PROP_H
#define PROP_H

#include <stdio.h>

/* 非空二叉树：n = n0 + n1 + n2，并且 n0 = n2 + 1。
 * 未知量用 -1。n 不超过 100000。返回 0；输入太大返回 -1。 */

typedef struct {
    int status; /* 1 唯一，0 有限多个，2 还有自由变量，-1 矛盾 */
    const char *reason;
    int count;
    int n, n0, n1, n2;
    int n0_min, n0_max;
    int n1_min, n1_max;
    int n2_min, n2_max;
} PropOut;

int prop_solve(int n, int n0, int n1, int n2, PropOut *out);

void prop_print(int n, int n0, int n1, int n2, FILE *out);

int prop_floor_log2(int n);

/* n >= 1。空树高度另计为 0，不进这里。 */
int prop_height_min(int n);

typedef struct {
    int n0, n1, n2, height, last_branch, leaves;
    int bottom, parents, prev_leaves, leaf_sum;
    int has_layers;
} CbtStat;

int cbt_stat(int n, CbtStat *out);

typedef struct {
    int parent, left, right, level, leaf, side, sibling;
    int parent0, left0, right0, i0;
} CbtAt;

/* side：0 根，1 左孩子，2 右孩子。不存在的编号是 0；0 起的下标没有时是 -1。 */
int cbt_at(int n, int i, CbtAt *out);

void cbt_print(int n, int i, int has_i, FILE *out);

typedef struct {
    int width, branch, short_nodes;
    int tall;
    int tall_full, child_min, child_max, tall_min, tall_max;
    int min_nodes, max_nodes;
} MostOut;

int most_compute(int level, int leaves, MostOut *out);

void most_print(int level, int leaves, FILE *out);

void range_print(int n, FILE *out);

void full_print(int h, FILE *out);

#endif
