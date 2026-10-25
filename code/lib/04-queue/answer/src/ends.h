#ifndef ENDS_H
#define ENDS_H

#include <stdbool.h>
#include <stdio.h>

/* 2014-3 的双端循环队列。end1 是 front，指向队头元素；
 * end2 是 rear，指向队尾元素的下一空位。两端都能入队、出队。
 * 仍牺牲一个单元：队空 front == rear，队满 (rear + 1) % cap == front，
 * 最多 cap - 1 个。从队头入队或从队尾出队时指针要后退，
 * 后退写成 (i - 1 + cap) % cap，避免负数取模。 */
#define ENDS_MAX 64

typedef struct {
    int data[ENDS_MAX];
    int cap;
    int front;
    int rear;
    FILE *trace;
} EndsQ;

typedef struct {
    int real_empty;
    int real_full;
    int a_empty;
    int a_full;
    int b_empty;
    int b_full;
    int c_empty;
    int c_full;
    int d_empty;
    int d_full;
} EndsJudge;

bool ends_init(EndsQ *Q, int cap);

bool ends_empty(const EndsQ *Q);

bool ends_full(const EndsQ *Q);

int ends_count(const EndsQ *Q);

bool ends_enq_rear(EndsQ *Q, int e);

bool ends_enq_front(EndsQ *Q, int e);

bool ends_deq_front(EndsQ *Q, int *out);

bool ends_deq_rear(EndsQ *Q, int *out);

void ends_print_cells(const EndsQ *Q, FILE *out);

/* 按 2014-3 四个选项计算。cap >= 2，指针要落在 0..cap-1。失败返回 false。 */
bool ends_judge(int cap, int end1, int end2, EndsJudge *out);

#endif
