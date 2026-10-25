#ifndef LINKQ_H
#define LINKQ_H

#include <stdbool.h>
#include <stdio.h>

/* 链队列，带头结点。front 和 rear 都指向结点。
 * 空队列时两个指针都指向头结点，判空是 front == rear。
 * 头结点让入队不必区分“原来是空的”；出队删掉最后一个数据结点时，
 * rear 必须回到头结点，否则它指着已经 free 的结点。 */
typedef struct LNode {
    int data;
    struct LNode *next;
} LNode;

typedef struct {
    LNode *front;
    LNode *rear;
    FILE *trace;
} LinkQ;

bool link_init(LinkQ *Q);

void link_destroy(LinkQ *Q);

bool link_empty(const LinkQ *Q);

int link_count(const LinkQ *Q);

bool link_enq(LinkQ *Q, int e);

bool link_deq(LinkQ *Q, int *out);

bool link_head(const LinkQ *Q, int *out);

void link_print(const LinkQ *Q, FILE *out);

#endif
