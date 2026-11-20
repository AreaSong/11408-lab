#ifndef CYCQ_H
#define CYCQ_H

#include <stdbool.h>
#include <stdio.h>

/* 循环队列，牺牲一个单元判满。王道约定：
 * front 指向队头元素，rear 指向队尾元素的下一空位，初始都是 0。
 * 队空 front == rear；队满 (rear + 1) % cap == front。
 * 因此最多放 cap - 1 个元素，rear 那一格永远留着，用来和队空区分。
 * 元素个数 (rear - front + cap) % cap。先加 cap 再取模，
 * rear 绕回到 front 前面时差是负的；C 的 % 对负数余数仍是负的。
 * cap 至少为 2。cap 为 1 时队空和队满会同时成立。 */
#define CYC_MAX 64
#define CYC_DEMO_CAP 8

typedef struct {
    int data[CYC_MAX];
    int cap;
    int front;
    int rear;
    FILE *trace;
} CycQ;

int cyc_length(int cap, int front, int rear);

bool cyc_init(CycQ *Q, int cap);

bool cyc_empty(const CycQ *Q);

bool cyc_full(const CycQ *Q);

int cyc_count(const CycQ *Q);

bool cyc_enq(CycQ *Q, int e);

/* 失败时不改 *out。 */
bool cyc_deq(CycQ *Q, int *out);

bool cyc_head(const CycQ *Q, int *out);

/* 队尾元素在 (rear - 1 + cap) % cap，不在 rear。rear 指着空位。 */
bool cyc_tail(const CycQ *Q, int *out, int *index);

void cyc_print_cells(const CycQ *Q, FILE *out);

#endif
