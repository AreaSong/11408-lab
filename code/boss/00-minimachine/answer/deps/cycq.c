#include "cycq.h"

int cyc_length(int cap, int front, int rear) {
    return (rear - front + cap) % cap;
}

bool cyc_init(CycQ *Q, int cap) {
    if (Q == NULL || cap < 2 || cap > CYC_MAX) {
        return false;
    }
    Q->cap = cap;
    Q->front = 0;
    Q->rear = 0;
    Q->trace = NULL;
    return true;
}

bool cyc_empty(const CycQ *Q) {
    return Q == NULL || Q->front == Q->rear;
}

bool cyc_full(const CycQ *Q) {
    return Q != NULL && (Q->rear + 1) % Q->cap == Q->front;
}

int cyc_count(const CycQ *Q) {
    if (Q == NULL) {
        return 0;
    }
    return cyc_length(Q->cap, Q->front, Q->rear);
}

static int occupied(const CycQ *Q, int i) {
    if (Q->front == Q->rear) {
        return 0;
    }
    if (Q->front < Q->rear) {
        return i >= Q->front && i < Q->rear;
    }
    /* rear 已经绕回，元素从 front 一直铺到数组末尾，再从 0 铺到 rear。 */
    return i >= Q->front || i < Q->rear;
}

void cyc_print_cells(const CycQ *Q, FILE *out) {
    fputc('[', out);
    for (int i = 0; i < Q->cap; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        if (occupied(Q, i)) {
            fprintf(out, "%d", Q->data[i]);
        } else {
            fputc('_', out);
        }
    }
    fputc(']', out);
}

bool cyc_enq(CycQ *Q, int e) {
    if (Q == NULL || cyc_full(Q)) {
        return false;
    }
    int old = Q->rear;
    /* 先写入 rear 指的空位，再前进。和“rear 指向元素”的那套顺序相反。 */
    Q->data[Q->rear] = e;
    Q->rear = (Q->rear + 1) % Q->cap;
    if (Q->trace != NULL) {
        fprintf(Q->trace, "enq e=%d front=%d rear=%d->%d count=%d full=%d cells=", e, Q->front, old,
                Q->rear, cyc_count(Q), cyc_full(Q) ? 1 : 0);
        cyc_print_cells(Q, Q->trace);
        fputc('\n', Q->trace);
    }
    return true;
}

bool cyc_deq(CycQ *Q, int *out) {
    if (cyc_empty(Q)) {
        return false;
    }
    int old = Q->front;
    int value = Q->data[Q->front];
    Q->front = (Q->front + 1) % Q->cap;
    if (out != NULL) {
        *out = value;
    }
    if (Q->trace != NULL) {
        fprintf(Q->trace, "deq e=%d front=%d->%d rear=%d count=%d full=%d cells=", value, old, Q->front,
                Q->rear, cyc_count(Q), cyc_full(Q) ? 1 : 0);
        cyc_print_cells(Q, Q->trace);
        fputc('\n', Q->trace);
    }
    return true;
}

bool cyc_head(const CycQ *Q, int *out) {
    if (!cyc_empty(Q)) {
        if (out != NULL) {
            *out = Q->data[Q->front];
        }
        return true;
    }
    return false;
}

bool cyc_tail(const CycQ *Q, int *out, int *index) {
    if (cyc_empty(Q)) {
        return false;
    }
    int at = (Q->rear - 1 + Q->cap) % Q->cap;
    if (out != NULL) {
        *out = Q->data[at];
    }
    if (index != NULL) {
        *index = at;
    }
    return true;
}
