#include "sharestack.h"

void share_init(ShareStack *S) {
    if (S == NULL) {
        return;
    }
    S->top1 = -1;
    S->top2 = SHARE_CAP;
    S->trace = NULL;
}

bool share_full(const ShareStack *S) {
    return S != NULL && S->top1 + 1 == S->top2;
}

bool share_empty(const ShareStack *S, int which) {
    if (S == NULL) {
        return true;
    }
    if (which == 1) {
        return S->top1 < 0;
    }
    if (which == 2) {
        return S->top2 >= SHARE_CAP;
    }
    return true;
}

int share_size(const ShareStack *S, int which) {
    if (S == NULL) {
        return 0;
    }
    if (which == 1) {
        return S->top1 + 1;
    }
    if (which == 2) {
        return SHARE_CAP - S->top2;
    }
    return 0;
}

static bool which_ok(int which) {
    return which == 1 || which == 2;
}

static void trace_move(ShareStack *S, const char *op, int which, int e, int old1, int old2) {
    if (S->trace == NULL) {
        return;
    }
    fprintf(S->trace, "%s stack=%d e=%d top1=%d->%d top2=%d->%d full=%d cells=",
            op, which, e, old1, S->top1, old2, S->top2, share_full(S) ? 1 : 0);
    share_print_cells(S, S->trace);
    fputc('\n', S->trace);
}

bool share_push(ShareStack *S, int which, int e) {
    int old1 = 0;
    int old2 = 0;
    if (S == NULL || !which_ok(which) || share_full(S)) {
        return false;
    }
    /* 满的判断必须是相邻，不能等 top1 == top2。
     * 相邻时再进一格，写下去的就是对方的栈顶。 */
    old1 = S->top1;
    old2 = S->top2;
    if (which == 1) {
        S->data[++S->top1] = e;
    } else {
        S->data[--S->top2] = e;
    }
    trace_move(S, "push", which, e, old1, old2);
    return true;
}

bool share_pop(ShareStack *S, int which, int *out) {
    int old1 = 0;
    int old2 = 0;
    int value = 0;
    if (S == NULL || !which_ok(which) || share_empty(S, which)) {
        return false;
    }
    old1 = S->top1;
    old2 = S->top2;
    if (which == 1) {
        value = S->data[S->top1--];
    } else {
        value = S->data[S->top2++];
    }
    if (out != NULL) {
        *out = value;
    }
    trace_move(S, "pop", which, value, old1, old2);
    return true;
}

bool share_top(const ShareStack *S, int which, int *out) {
    if (share_empty(S, which) || !which_ok(which)) {
        return false;
    }
    if (out != NULL) {
        *out = (which == 1) ? S->data[S->top1] : S->data[S->top2];
    }
    return true;
}

void share_print_cells(const ShareStack *S, FILE *out) {
    fputc('[', out);
    if (S != NULL) {
        for (int i = 0; i < SHARE_CAP; i++) {
            int used = (i <= S->top1) || (i >= S->top2);
            if (i > 0) {
                fputc(' ', out);
            }
            if (used) {
                fprintf(out, "%d", S->data[i]);
            } else {
                fputc('_', out);
            }
        }
    }
    fputc(']', out);
}
