#ifndef SHARESTACK_H
#define SHARESTACK_H

#include <stdbool.h>
#include <stdio.h>

/* 两个栈共用 data[0..SHARE_CAP-1]，栈底分别钉在数组两端，向中间长。
 * top1、top2 都指向本栈的栈顶元素：栈 1 空时 top1 == -1，向右长；
 * 栈 2 空时 top2 == SHARE_CAP，向左长。
 * 栈满是 top1 + 1 == top2，这时空闲格为 0。不要写成 top1 == top2，
 * 那会晚拦一步，把对方的栈顶盖掉。两个指针各管各的空满，不用像循环队列那样牺牲一格。
 * 容量取 8 是为了几次 push 就能看到栈满，条件和 MaxSize 更大时相同。 */
#define SHARE_CAP 8

typedef struct {
    int data[SHARE_CAP];
    int top1;
    int top2;
    FILE *trace;
} ShareStack;

void share_init(ShareStack *S);

bool share_full(const ShareStack *S);

bool share_empty(const ShareStack *S, int which);

int share_size(const ShareStack *S, int which);

bool share_push(ShareStack *S, int which, int e);

bool share_pop(ShareStack *S, int which, int *out);

bool share_top(const ShareStack *S, int which, int *out);

/* 按下标从左到右打印。逻辑上空着的格子写成 _，不管里面还留着旧值。 */
void share_print_cells(const ShareStack *S, FILE *out);

#endif
