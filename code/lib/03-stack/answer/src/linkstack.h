#ifndef LINKSTACK_H
#define LINKSTACK_H

#include <stdbool.h>
#include <stdio.h>

/* 链栈不带头结点。栈只在栈顶插入和删除，头结点省不出按位找前驱的方便，
 * 王道因此通常不带头结点。top 直接指向栈顶结点，空栈是 top == NULL。
 * 进栈是单链表的头插，出栈是删掉第一个结点。只要内存够就没有栈满。 */
typedef struct LNode {
    int data;
    struct LNode *next;
} LNode;

typedef struct {
    LNode *top;
    FILE *trace;
} LinkStack;

bool link_init(LinkStack *S);

void link_destroy(LinkStack *S);

bool link_empty(const LinkStack *S);

int link_size(const LinkStack *S);

bool link_push(LinkStack *S, int e);

bool link_pop(LinkStack *S, int *out);

bool link_top(const LinkStack *S, int *out);

/* 从栈底打印到栈顶。链上的 next 是从栈顶走向栈底，所以要倒着收集。 */
void link_print(const LinkStack *S, FILE *out);

#endif
