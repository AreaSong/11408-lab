#ifndef SEQLIST_H
#define SEQLIST_H

#include <stdbool.h>
#include <stdio.h>

/* 动态顺序表。位序从 1 开始，数组下标从 0 开始，与王道 SqList 一致。
 * capacity 即王道的 MaxSize。初始容量取 4，是为了少量插入就能看到加倍；
 * 书上的 InitSize 是符号常量，加倍策略相同。
 * trace 不属于顺序表的逻辑结构，只用来打印插入、删除时的元素移动。 */
#define SEQLIST_INIT_CAP 4

typedef struct {
    int *data;
    int length;
    int capacity;
    FILE *trace;
} SeqList;

/* 分配初始容量，length 为 0。失败返回 false，此时 destroy 仍可调用。 */
bool seqlist_init(SeqList *L);

void seqlist_destroy(SeqList *L);

/* 容量加倍。元素个数不变，所以这次复制不算进插入、删除的移动次数。 */
bool seqlist_grow(SeqList *L);

/* 在位序 i 处插入 e（1 ≤ i ≤ length+1）。满了先扩容。
 * moves 非 NULL 时写入后移的元素个数；失败时不改 *moves。 */
bool seqlist_insert(SeqList *L, int i, int e, int *moves);

/* 删除位序 i（1 ≤ i ≤ length），被删元素写入 *out（可为 NULL）。 */
bool seqlist_delete(SeqList *L, int i, int *out, int *moves);

/* 按值查找第一个等于 e 的元素，返回位序；没有返回 0。 */
int seqlist_locate(const SeqList *L, int e);

/* 按位取值。失败时不改 *out。 */
bool seqlist_get(const SeqList *L, int i, int *out);

/* 打印成 [1 2 3]，空表是 []。 */
void seqlist_print(const SeqList *L, FILE *out);

#endif
