#ifndef HUFF_H
#define HUFF_H

#include <stdio.h>

/*
 * 哈夫曼树用双亲、左右孩子的静态数组，下标从 1 起。
 * 叶子占 1..n，其后每合并一次占一个新下标，共 2n-1 个结点。
 * 左分支记 0，右分支记 1。权值相等时下标小的先选出，并作为左孩子。
 */

#define HUFF_MAX 16
#define HUFF_NODE (HUFF_MAX * 2)
#define HUFF_CODE 16

enum { HUFF_OK = 0, HUFF_RANGE = 1, HUFF_BAD = 2, HUFF_PREFIX = 3 };

typedef struct {
    int weight;
    int parent;
    int left;
    int right;
    char name;
} HuffNode;

typedef struct {
    HuffNode node[HUFF_NODE];
    char code[HUFF_MAX + 1][HUFF_CODE];
    int nleaf;
    int nnode;
} Huff;

int huff_build(Huff *t, const char *names, const int *weight, int n, FILE *out);
int huff_wpl(const Huff *t);
int huff_inside(const Huff *t);
int huff_long(const Huff *t, int min_len);
int huff_degree1(const Huff *t);
int huff_height(const Huff *t);

/* codes 每行一个码字。bad_a、bad_b 可为 NULL。返回 HUFF_OK 或 HUFF_PREFIX。 */
int code_prefix(const char codes[][HUFF_CODE], int n, char *bad_a, char *bad_b);
int code_decode(const char *bits, const char *names, const char codes[][HUFF_CODE], int n,
                char *text, int cap);

/* 两条从根到叶的权值路径能否出现在同一棵哈夫曼树上。1 能，0 不能。 */
int huff_paths_same(const int *a, int na, const int *b, int nb);

#endif
