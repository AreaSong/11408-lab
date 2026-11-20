#ifndef DLIST_H
#define DLIST_H

#include <stdbool.h>
#include <stdio.h>

/* 带头结点的双链表。circular 为真是循环双链表：头结点的 prior 指向尾，
 * 尾的 next 指向头，空表时 head->next 和 head->prior 都指向自己，
 * 判空看 head->next == head。
 * circular 为假是非循环双链表：空表 head->next == NULL，头结点 prior 保持 NULL，
 * 插到表尾或删表尾时后继是 NULL，不能去写它的 prior。
 * 位序从 1 开始。已知结点时改四个指针是 O(1)，找第 i 个仍是 O(n)。 */
typedef struct DNode {
    int data;
    struct DNode *prior;
    struct DNode *next;
} DNode;

typedef struct {
    DNode *head;
    FILE *trace;
    bool circular;
} DList;

bool dlist_init(DList *L, bool circular);

void dlist_destroy(DList *L);

bool dlist_empty(const DList *L);

int dlist_length(const DList *L);

/* 每个结点都满足 prior/next 互指。环被写断或非循环表的头结点 prior 被改掉都会失败。 */
bool dlist_links_ok(const DList *L);

bool dlist_insert(DList *L, int i, int e);

bool dlist_delete(DList *L, int i, int *out);

int dlist_locate(const DList *L, int e);

bool dlist_get(const DList *L, int i, int *out);

void dlist_print(const DList *L, FILE *out);

void dlist_status(const DList *L, FILE *out);

#endif
