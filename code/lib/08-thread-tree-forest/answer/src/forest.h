#ifndef FOREST_H
#define FOREST_H

#include "thr.h"

#include <stdio.h>

/* 树和森林。同一份链接读两遍：
 * first/next 是孩子兄弟表示，left/right 是对应的二叉树。
 * 下标从 1 起，0 是空链。根的 parent 为 -1，这是王道双亲表示的约定。 */
#define FOR_MAX 64

typedef struct {
    char data;
    int parent;
    int first;
    int next;
} ForNode;

typedef struct {
    ForNode node[FOR_MAX];
    int n;
    int ntree;
    int root[FOR_MAX];
} Forest;

typedef struct {
    int total;
    int as_parent;
    int as_sibling;
    int as_uncle;
    int as_other;
    int as_iii;
} GrandStats;

void forest_init(Forest *f);

/* 形如 A(B(D,E),C) F(G)。树之间可以有空格，孩子用逗号分开。 */
int forest_parse(Forest *f, const char *s);

/* 先序和中序唯一确定二叉树，再按左孩子右兄弟读回森林。 */
int forest_from_bi(Forest *f, const char *pre, const char *inord);

int forest_find(const Forest *f, char data);
int forest_bin_parent(const Forest *f, int id);
void forest_grand_stats(const Forest *f, GrandStats *stats);

int forest_leaves(const Forest *f);
int forest_right_null(const Forest *f);
int forest_bin_leaf(const Forest *f);
int forest_deg1(const Forest *f);

int forest_seq(const Forest *f, char *pre, char *inord, char *post, int cap);
void forest_show(const Forest *f, FILE *out);

#endif
