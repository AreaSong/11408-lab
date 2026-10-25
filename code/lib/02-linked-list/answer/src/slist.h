#ifndef SLIST_H
#define SLIST_H

#include <stdbool.h>
#include <stdio.h>

/* 带头结点的单链表。教科书里的 LinkList L 就是这里的 head：
 * head 不存元素，空表的判空条件是 head->next == NULL（不是 head == NULL，
 * 头结点在初始化时就已经分配了）。位序从 1 开始，头结点算位序 0。
 * tail 指向最后一个结点，空表时 tail == head，这样尾插不用每次从头扫描。
 * trace 不属于逻辑结构，只用来打印指针怎么改。 */
typedef struct SNode {
    int data;
    struct SNode *next;
} SNode;

typedef struct {
    SNode *head;
    SNode *tail;
    FILE *trace;
} SList;

/* 分配头结点。失败时 head 为 NULL，destroy 仍可调用。 */
bool slist_init(SList *L);

void slist_destroy(SList *L);

bool slist_empty(const SList *L);

int slist_length(const SList *L);

/* 头插：新结点放在头结点后面，所以连续头插得到的顺序与插入顺序相反。 */
bool slist_head_insert(SList *L, int e);

/* 尾插：挂到 tail 后面。空表时 tail 就是头结点。 */
bool slist_tail_insert(SList *L, int e);

/* 在位序 i 插入（1 ≤ i ≤ length+1）。先找到位序 i-1 的前驱。 */
bool slist_insert(SList *L, int i, int e);

/* 删除位序 i（1 ≤ i ≤ length）。被删元素写入 *out（可为 NULL）。 */
bool slist_delete(SList *L, int i, int *out);

/* 按值查找第一个等于 e 的位序；没有返回 0。 */
int slist_locate(const SList *L, int e);

/* 按位取值。单链表没有按下标直接跳到第 i 个的能力，必须沿 next 走。
 * 失败时不改 *out。 */
bool slist_get(const SList *L, int i, int *out);

/* 带头结点的原地逆置：原第一个结点留下当新表尾，后面的结点逐个头插。 */
void slist_reverse(SList *L);

void slist_print(const SList *L, FILE *out);

#endif
