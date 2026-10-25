/*
 * 共享栈。top1 初值 -1，向右长；top2 初值 SHARE_CAP，向左长。
 * 栈满条件是 top1 + 1 == top2。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "sharestack.h"

static void print_state(const ShareStack *S) {
    printf("state top1=%d top2=%d full=%d size1=%d size2=%d cells=", S->top1, S->top2,
           share_full(S) ? 1 : 0, share_size(S, 1), share_size(S, 2));
    share_print_cells(S, stdout);
    printf("\n");
}

static int push_share(ShareStack *S) {
    int which = 0;
    int e = 0;
    if (scanf("%d %d", &which, &e) != 2) {
        return 0;
    }
    if (which != 1 && which != 2) {
        printf("fail stack=%d\n", which);
        return 1;
    }
    if (!share_push(S, which, e)) {
        printf("fail push full stack=%d e=%d top1=%d top2=%d cond=top1+1==top2\n", which, e, S->top1,
               S->top2);
        return 1;
    }
    printf("ok push stack=%d e=%d size1=%d size2=%d\n", which, e, share_size(S, 1), share_size(S, 2));
    return 1;
}

static int pop_share(ShareStack *S) {
    int which = 0;
    int e = 0;
    if (scanf("%d", &which) != 1) {
        return 0;
    }
    if (which != 1 && which != 2) {
        printf("fail stack=%d\n", which);
        return 1;
    }
    if (!share_pop(S, which, &e)) {
        printf("fail pop empty stack=%d top1=%d top2=%d\n", which, S->top1, S->top2);
        return 1;
    }
    printf("ok pop stack=%d e=%d size1=%d size2=%d\n", which, e, share_size(S, 1), share_size(S, 2));
    return 1;
}

static int top_share(ShareStack *S) {
    int which = 0;
    int e = 0;
    if (scanf("%d", &which) != 1) {
        return 0;
    }
    if (which != 1 && which != 2) {
        printf("fail stack=%d\n", which);
        return 1;
    }
    if (!share_top(S, which, &e)) {
        printf("fail top empty stack=%d\n", which);
        return 1;
    }
    if (which == 1) {
        printf("ok top stack=1 e=%d top1=%d\n", e, S->top1);
    } else {
        printf("ok top stack=2 e=%d top2=%d\n", e, S->top2);
    }
    return 1;
}

static int empty_share(ShareStack *S) {
    int which = 0;
    if (scanf("%d", &which) != 1) {
        return 0;
    }
    if (which == 1) {
        printf("ok empty stack=1 empty=%d top1=%d cond=top1==-1\n", share_empty(S, 1) ? 1 : 0, S->top1);
        return 1;
    }
    if (which == 2) {
        printf("ok empty stack=2 empty=%d top2=%d cond=top2==%d\n", share_empty(S, 2) ? 1 : 0, S->top2,
               SHARE_CAP);
        return 1;
    }
    printf("fail stack=%d\n", which);
    return 1;
}

static int dispatch(ShareStack *S, const char *cmd) {
    if (strcmp(cmd, "push") == 0) {
        return push_share(S);
    }
    if (strcmp(cmd, "pop") == 0) {
        return pop_share(S);
    }
    if (strcmp(cmd, "top") == 0) {
        return top_share(S);
    }
    if (strcmp(cmd, "empty") == 0) {
        return empty_share(S);
    }
    if (strcmp(cmd, "state") == 0) {
        print_state(S);
        return 1;
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    ShareStack stack;
    char cmd[16];
    share_init(&stack);
    stack.trace = stdout;
    printf("init top1=%d top2=%d max=%d full_when=top1+1==top2 cells=", stack.top1, stack.top2, SHARE_CAP);
    share_print_cells(&stack, stdout);
    printf("\n");
    for (;;) {
        prompt("请输入命令（push k e / pop k / top k / empty k / state，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(&stack, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
