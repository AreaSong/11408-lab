#ifndef UFSET_H
#define UFSET_H

#include <stdio.h>

/*
 * 并查集用双亲表示。结点编号 1..n，根的 parent 等于自己。
 * UF_PLAIN：把第一个参数所在的树挂到第二个的根下，可以退化为一条链。
 * UF_RANK：按秩合并。秩相等时把后一个根挂到前一个根下，前一个根的秩加 1。
 * UF_BOTH：按秩合并，并且查找时做路径压缩。
 * 秩是树高的上界。路径压缩会把真实高度压低，但不会把秩改回去。
 * height 和秩用同一把尺子：单结点是 0，每往下一条边加 1。
 */

#define UF_MAX 32

enum { UF_PLAIN = 0, UF_RANK = 1, UF_BOTH = 2 };
enum { UF_OK = 0, UF_RANGE = 1 };

typedef struct {
    int parent[UF_MAX + 1];
    int rank[UF_MAX + 1];
    int n;
    int mode;
    int compress;
} Uf;

void uf_begin(Uf *u, int n, int mode, FILE *out);
int uf_find(Uf *u, int x, FILE *out);
int uf_union(Uf *u, int x, int y, FILE *out);
void uf_dump(const Uf *u, FILE *out);
int uf_height(const Uf *u);
int uf_components(const Uf *u);

/* 无向图。两端已经连通的边就是环，这条边不加入。 */
int uf_cycle(int n, const int *a, const int *b, int m, FILE *out);

#endif
