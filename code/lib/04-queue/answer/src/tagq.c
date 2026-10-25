#include "tagq.h"

bool tag_init(TagQ *Q, int cap) {
    if (Q == NULL || cap < 2 || cap > TAG_MAX) {
        return false;
    }
    Q->cap = cap;
    Q->front = 0;
    Q->rear = 0;
    /* 初始看成“刚出过队”。写成 1 的话，空队列满足队满，第一个元素进不去。 */
    Q->tag = 0;
    return true;
}

bool tag_empty(const TagQ *Q) {
    return Q == NULL || (Q->front == Q->rear && Q->tag == 0);
}

bool tag_full(const TagQ *Q) {
    return Q != NULL && Q->front == Q->rear && Q->tag == 1;
}

int tag_raw(const TagQ *Q) {
    if (Q == NULL) {
        return 0;
    }
    return (Q->rear - Q->front + Q->cap) % Q->cap;
}

int tag_count(const TagQ *Q) {
    if (Q == NULL) {
        return 0;
    }
    if (Q->front != Q->rear) {
        return tag_raw(Q);
    }
    return Q->tag == 1 ? Q->cap : 0;
}

bool tag_enq(TagQ *Q, int e) {
    if (Q == NULL || tag_full(Q)) {
        return false;
    }
    Q->data[Q->rear] = e;
    Q->rear = (Q->rear + 1) % Q->cap;
    Q->tag = 1;
    return true;
}

bool tag_deq(TagQ *Q, int *out) {
    if (tag_empty(Q)) {
        return false;
    }
    int value = Q->data[Q->front];
    Q->front = (Q->front + 1) % Q->cap;
    Q->tag = 0;
    if (out != NULL) {
        *out = value;
    }
    return true;
}

static int holds(const TagQ *Q, int i) {
    if (Q->front == Q->rear) {
        return Q->tag == 1;
    }
    if (Q->front < Q->rear) {
        return i >= Q->front && i < Q->rear;
    }
    return i >= Q->front || i < Q->rear;
}

void tag_print_cells(const TagQ *Q, FILE *out) {
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
