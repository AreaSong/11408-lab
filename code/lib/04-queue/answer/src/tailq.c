#include "tailq.h"

#include <stddef.h>

bool tail_init(TailQ *Q, int cap) {
    if (Q == NULL || cap < 2 || cap > TAIL_MAX) {
        return false;
    }
    Q->cap = cap;
    Q->front = 0;
    Q->rear = cap - 1;
    return true;
}

bool tail_empty(const TailQ *Q) {
    return Q != NULL && (Q->rear + 1) % Q->cap == Q->front;
}

bool tail_full(const TailQ *Q) {
    return Q != NULL && (Q->rear + 2) % Q->cap == Q->front;
}

int tail_count(const TailQ *Q) {
    if (Q == NULL || tail_empty(Q)) {
        return 0;
    }
    return (Q->rear - Q->front + Q->cap) % Q->cap + 1;
}

bool tail_enq(TailQ *Q, int e) {
    if (Q == NULL || tail_full(Q)) {
        return false;
    }
    /* rear 指向的是元素本身，写入前要先空出下一格。 */
    Q->rear = (Q->rear + 1) % Q->cap;
    Q->data[Q->rear] = e;
    return true;
}
