#ifndef CLIST_H
#define CLIST_H

#include <stdbool.h>
#include <stdio.h>

/* 带头结点的循环单链表。尾结点的 next 指回头结点，链上没有 NULL。
 * 判空是 head->next == head，不是 head->next == NULL。
 * rear 指向尾结点，空表时 rear == head，并且 rear->next == head 始终成立。
 * 只拿头指针的话，找尾是 O(n)；维护 rear 之后尾插是 O(1)。 */
typedef struct CNode {
    int data;
    struct CNode *next;
} CNode;

typedef struct {
    CNode *head;
    CNode *rear;
    FILE *trace;
} CList;

bool clist_init(CList *L);

void clist_destroy(CList *L);

bool clist_empty(const CList *L);

int clist_length(const CList *L);

bool clist_ring_ok(const CList *L);

bool clist_head_insert(CList *L, int e);

bool clist_tail_insert(CList *L, int e);

bool clist_insert(CList *L, int i, int e);

bool clist_delete(CList *L, int i, int *out);

int clist_locate(const CList *L, int e);

bool clist_get(const CList *L, int i, int *out);

void clist_print(const CList *L, FILE *out);

/* 打印判空用到的观察值：next=head 或 next=<第一个元素>。 */
void clist_status(const CList *L, FILE *out);

#endif
