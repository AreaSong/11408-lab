#ifndef HEAP_H
#define HEAP_H

#include <stdio.h>

/*
 * 顺序存储的堆。下标从 1 起，0 空着：父结点是 ⌊i/2⌋，左孩子 2i，右孩子 2i+1。
 * 这和王道堆排序的筛选下标一致。is_max 为 1 是大根堆，为 0 是小根堆。
 * 优先队列就是在堆顶取最值：小根堆的堆顶是当前最小。
 */

#define HEAP_MAX 32

enum { HEAP_OK = 0, HEAP_RANGE = 1, HEAP_EMPTY = 2, HEAP_FULL = 3, HEAP_BAD = 4 };

typedef struct {
    int key[HEAP_MAX + 1];
    int n;
    int is_max;
} Heap;

void heap_init(Heap *h, int is_max);
int heap_load(Heap *h, const int *keys, int n, int is_max);
int heap_ok(const Heap *h);
void heap_fprint(const Heap *h, FILE *out);

/* out 非空时打印每次筛选。建堆是自底向上，不是逐个插入。 */
int heap_build(Heap *h, const int *keys, int n, int is_max, FILE *out);
int heap_insert_all(Heap *h, const int *keys, int n, int is_max, FILE *out);
int heap_push(Heap *h, int key, FILE *out);
int heap_replace_root(Heap *h, int key, FILE *out);
int heap_pop(Heap *h, int *got, FILE *out);

#endif
