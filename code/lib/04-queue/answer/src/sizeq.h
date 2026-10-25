#ifndef SIZEQ_H
#define SIZEQ_H

#include <stdbool.h>
#include <stdio.h>

/* 同一套 front / rear，另用 size 记住元素个数。
 * 队空 size == 0，队满 size == cap，数组可以放满。
 * front == rear 时可能是空，也可能是满，要看 size，不能看指针。 */
#define SIZEQ_MAX 64

typedef struct {
    int data[SIZEQ_MAX];
    int cap;
    int front;
    int rear;
    int size;
} SizeQ;

bool size_init(SizeQ *Q, int cap);

bool size_empty(const SizeQ *Q);

bool size_full(const SizeQ *Q);

int size_count(const SizeQ *Q);

bool size_enq(SizeQ *Q, int e);

bool size_deq(SizeQ *Q, int *out);

void size_print_cells(const SizeQ *Q, FILE *out);

#endif
