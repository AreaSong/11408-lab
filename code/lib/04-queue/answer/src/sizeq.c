#include "sizeq.h"

bool size_init(SizeQ *Q, int cap) {
    if (Q == NULL || cap < 2 || cap > SIZEQ_MAX) {
        return false;
    }
    Q->cap = cap;
    Q->front = 0;
    Q->rear = 0;
    Q->size = 0;
    return true;
}

bool size_empty(const SizeQ *Q) {
    return Q == NULL || Q->size == 0;
}

bool size_full(const SizeQ *Q) {
    return Q != NULL && Q->size == Q->cap;
}

int size_count(const SizeQ *Q) {
    if (Q == NULL) {
        return 0;
    }
    return Q->size;
}

bool size_enq(SizeQ *Q, int e) {
    if (Q == NULL || size_full(Q)) {
        return false;
    }
    Q->data[Q->rear] = e;
    Q->rear = (Q->rear + 1) % Q->cap;
    Q->size++;
    return true;
}

bool size_deq(SizeQ *Q, int *out) {
    if (size_empty(Q)) {
        return false;
    }
    int value = Q->data[Q->front];
    Q->front = (Q->front + 1) % Q->cap;
    Q->size--;
    if (out != NULL) {
        *out = value;
    }
    return true;
}

static int holds(const SizeQ *Q, int i) {
    if (Q->size == 0) {
        return 0;
    }
    /* 放满时 front == rear，按“没放满”的区间去数会得到 0 格。 */
    if (Q->size == Q->cap) {
        return 1;
    }
    if (Q->front < Q->rear) {
        return i >= Q->front && i < Q->rear;
    }
    return i >= Q->front || i < Q->rear;
}

void size_print_cells(const SizeQ *Q, FILE *out) {
    fputc('[', out);
    for (int i = 0; i < Q->cap; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        if (holds(Q, i)) {
            fprintf(out, "%d", Q->data[i]);
        } else {
            fputc('_', out);
        }
    }
    fputc(']', out);
}
