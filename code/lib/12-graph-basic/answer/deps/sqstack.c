#include "sqstack.h"

bool sq_init(SqStack *S, int cap) {
    if (S == NULL || cap < 1 || cap > SQ_MAX) {
        return false;
    }
    S->top = -1;
    S->cap = cap;
    S->trace = NULL;
    return true;
}

bool sq_empty(const SqStack *S) {
    return S == NULL || S->top < 0;
}

bool sq_full(const SqStack *S) {
    return S != NULL && S->top >= S->cap - 1;
}

int sq_size(const SqStack *S) {
    if (sq_empty(S)) {
        return 0;
    }
    return S->top + 1;
}

bool sq_push(SqStack *S, int e) {
    if (S == NULL || sq_full(S)) {
        return false;
    }
    /* 先移动 top，再写入。写成 data[top++] 会在空栈时写到 data[-1]。 */
    int old = S->top;
    S->data[++S->top] = e;
    if (S->trace != NULL) {
        fprintf(S->trace, "push e=%d top=%d->%d stack=", e, old, S->top);
        sq_print(S, S->trace);
        fputc('\n', S->trace);
    }
    return true;
}

bool sq_pop(SqStack *S, int *out) {
    if (sq_empty(S)) {
        return false;
    }
    int old = S->top;
    int value = S->data[S->top--];
    if (out != NULL) {
        *out = value;
    }
    if (S->trace != NULL) {
        fprintf(S->trace, "pop e=%d top=%d->%d stack=", value, old, S->top);
        sq_print(S, S->trace);
        fputc('\n', S->trace);
    }
    return true;
}

bool sq_top(const SqStack *S, int *out) {
    if (sq_empty(S)) {
        return false;
    }
    if (out != NULL) {
        *out = S->data[S->top];
    }
    return true;
}

void sq_print(const SqStack *S, FILE *out) {
    fputc('[', out);
    if (!sq_empty(S)) {
        for (int i = 0; i <= S->top; i++) {
            if (i > 0) {
                fputc(' ', out);
            }
            fprintf(out, "%d", S->data[i]);
        }
    }
    fputc(']', out);
}
