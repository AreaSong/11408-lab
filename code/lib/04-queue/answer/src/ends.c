#include "ends.h"

static int back(int i, int cap) {
    return (i - 1 + cap) % cap;
}

bool ends_init(EndsQ *Q, int cap) {
    if (Q == NULL || cap < 2 || cap > ENDS_MAX) {
        return false;
    }
    Q->cap = cap;
    Q->front = 0;
    Q->rear = 0;
    Q->trace = NULL;
    return true;
}

bool ends_empty(const EndsQ *Q) {
    return Q == NULL || Q->front == Q->rear;
}

bool ends_full(const EndsQ *Q) {
    return Q != NULL && (Q->rear + 1) % Q->cap == Q->front;
}

int ends_count(const EndsQ *Q) {
    if (ends_empty(Q)) {
        return 0;
    }
    return (Q->rear - Q->front + Q->cap) % Q->cap;
}

static int occupied(const EndsQ *Q, int i) {
    if (Q->front == Q->rear) {
        return 0;
    }
    if (Q->front < Q->rear) {
        return i >= Q->front && i < Q->rear;
    }
    return i >= Q->front || i < Q->rear;
}

void ends_print_cells(const EndsQ *Q, FILE *out) {
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

static void trace_op(EndsQ *Q, const char *op, int e, int has_e, int old_front, int old_rear) {
    if (Q->trace == NULL) {
        return;
    }
    if (has_e) {
        fprintf(Q->trace, "%s e=%d ", op, e);
    } else {
        fprintf(Q->trace, "%s ", op);
    }
    fprintf(Q->trace, "front=%d->%d rear=%d->%d count=%d full=%d cells=", old_front, Q->front, old_rear,
            Q->rear, ends_count(Q), ends_full(Q) ? 1 : 0);
    ends_print_cells(Q, Q->trace);
    fputc('\n', Q->trace);
}

bool ends_enq_rear(EndsQ *Q, int e) {
    if (Q == NULL || ends_full(Q)) {
        return false;
    }
    int old_front = Q->front;
    int old_rear = Q->rear;
    Q->data[Q->rear] = e;
    Q->rear = (Q->rear + 1) % Q->cap;
    trace_op(Q, "enq-r", e, 1, old_front, old_rear);
    return true;
}

bool ends_enq_front(EndsQ *Q, int e) {
    if (Q == NULL || ends_full(Q)) {
        return false;
    }
    int old_front = Q->front;
    int old_rear = Q->rear;
    Q->front = back(Q->front, Q->cap);
    Q->data[Q->front] = e;
    trace_op(Q, "enq-f", e, 1, old_front, old_rear);
    return true;
}

bool ends_deq_front(EndsQ *Q, int *out) {
    if (ends_empty(Q)) {
        return false;
    }
    int old_front = Q->front;
    int old_rear = Q->rear;
    int value = Q->data[Q->front];
    Q->front = (Q->front + 1) % Q->cap;
    if (out != NULL) {
        *out = value;
    }
    trace_op(Q, "deq-f", value, 1, old_front, old_rear);
    return true;
}

bool ends_deq_rear(EndsQ *Q, int *out) {
    if (ends_empty(Q)) {
        return false;
    }
    int old_front = Q->front;
    int old_rear = Q->rear;
    Q->rear = back(Q->rear, Q->cap);
    int value = Q->data[Q->rear];
    if (out != NULL) {
        *out = value;
    }
    trace_op(Q, "deq-r", value, 1, old_front, old_rear);
    return true;
}

bool ends_judge(int cap, int end1, int end2, EndsJudge *out) {
    if (out == NULL || cap < 2 || cap > ENDS_MAX || end1 < 0 || end1 >= cap || end2 < 0 || end2 >= cap) {
        return false;
    }
    out->real_empty = end1 == end2;
    out->real_full = (end2 + 1) % cap == end1;
    out->a_empty = end1 == end2;
    out->a_full = (end2 + 1) % cap == end1;
    out->b_empty = end1 == end2;
    out->b_full = end2 == (end1 + 1) % (cap - 1);
    /* 选项 C 的队空按试卷是 end2 == (end1 + 1) mod M。 */
    out->c_empty = end2 == (end1 + 1) % cap;
    out->c_full = (end2 + 1) % cap == end1;
    out->d_empty = (end2 + 1) % cap == end1;
    out->d_full = end2 == (end1 + 1) % (cap - 1);
    return true;
}
