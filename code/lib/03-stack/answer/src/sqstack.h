#ifndef SQSTACK_H
#define SQSTACK_H

#include <stdbool.h>
#include <stdio.h>

/* 顺序栈。王道约定：top 指向栈顶元素，空栈 top == -1。
 * 进栈先把 top 加 1 再写入，出栈先读出再把 top 减 1。
 * 另一种教材让 top 指向下一个空位，空栈是 0，两套只差 1，
 * 本关固定用王道这一套。栈满是 top == cap-1（上溢），
 * 元素个数是 top+1。cap 不超过 SQ_MAX，由调用方指定。
 * trace 不属于栈本身，只用来打印 top 怎么移动。 */
#define SQ_MAX 64
#define SQ_DEMO_CAP 8

typedef struct {
    int data[SQ_MAX];
    int top;
    int cap;
    FILE *trace;
} SqStack;

bool sq_init(SqStack *S, int cap);

bool sq_empty(const SqStack *S);

bool sq_full(const SqStack *S);

int sq_size(const SqStack *S);

bool sq_push(SqStack *S, int e);

/* 失败时不改 *out。 */
bool sq_pop(SqStack *S, int *out);

bool sq_top(const SqStack *S, int *out);

/* 从栈底打印到栈顶，空栈是 []。 */
void sq_print(const SqStack *S, FILE *out);

#endif
