/*
 * 顺序栈和链栈的命令解释器。
 * 第一行是种类：sq 或 link。后面是 push / pop / top / empty / size。
 * 顺序栈容量是 SQ_DEMO_CAP，top 从 -1 开始，指向栈顶元素。
 */
#include <stdio.h>
#include <string.h>

#include "linkstack.h"
#include "prompt.h"
#include "sqstack.h"

static void print_sq_ok(const char *verb, int e, int has_e, const SqStack *S) {
    if (has_e) {
        printf("ok %s e=%d size=%d stack=", verb, e, sq_size(S));
    } else {
        printf("ok %s size=%d stack=", verb, sq_size(S));
    }
    sq_print(S, stdout);
    printf("\n");
}

static int push_sq(SqStack *S) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!sq_push(S, e)) {
        printf("fail push full e=%d top=%d\n", e, S->top);
        return 1;
    }
    print_sq_ok("push", e, 1, S);
    return 1;
}

static int pop_sq(SqStack *S) {
    int e = 0;
    if (!sq_pop(S, &e)) {
        printf("fail pop empty top=%d\n", S->top);
        return 1;
    }
    print_sq_ok("pop", e, 1, S);
    return 1;
}

static int top_sq(SqStack *S) {
    int e = 0;
    if (!sq_top(S, &e)) {
        printf("fail top empty\n");
        return 1;
    }
    printf("ok top e=%d top=%d\n", e, S->top);
    return 1;
}

static int empty_sq(SqStack *S) {
    printf("ok empty=%d\n", sq_empty(S) ? 1 : 0);
    return 1;
}

static int size_sq(SqStack *S) {
    printf("ok size=%d top=%d\n", sq_size(S), S->top);
    return 1;
}

static int dispatch_sq(SqStack *S, const char *cmd) {
    if (strcmp(cmd, "push") == 0) {
        return push_sq(S);
    }
    if (strcmp(cmd, "pop") == 0) {
        return pop_sq(S);
    }
    if (strcmp(cmd, "top") == 0) {
        return top_sq(S);
    }
    if (strcmp(cmd, "empty") == 0) {
        return empty_sq(S);
    }
    if (strcmp(cmd, "size") == 0) {
        return size_sq(S);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

static int run_sq(void) {
    SqStack stack;
    char cmd[16];
    if (!sq_init(&stack, SQ_DEMO_CAP)) {
        return 1;
    }
    stack.trace = stdout;
    printf("kind=sq top=-1 cap=%d empty_when=top==-1 full_when=top==%d\n", stack.cap, stack.cap - 1);
    for (;;) {
        prompt("请输入命令（push e / pop / top / empty / size，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch_sq(&stack, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}

static void print_link_ok(const char *verb, int e, int has_e, const LinkStack *S) {
    if (has_e) {
        printf("ok %s e=%d size=%d stack=", verb, e, link_size(S));
    } else {
        printf("ok %s size=%d stack=", verb, link_size(S));
    }
    link_print(S, stdout);
    printf("\n");
}

static int push_link(LinkStack *S) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!link_push(S, e)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    print_link_ok("push", e, 1, S);
    return 1;
}

static int pop_link(LinkStack *S) {
    int e = 0;
    if (!link_pop(S, &e)) {
        printf("fail pop empty top=NULL\n");
        return 1;
    }
    print_link_ok("pop", e, 1, S);
    return 1;
}

static int top_link(LinkStack *S) {
    int e = 0;
    if (!link_top(S, &e)) {
        printf("fail top empty\n");
        return 1;
    }
    printf("ok top e=%d\n", e);
    return 1;
}

static int empty_link(LinkStack *S) {
    printf("ok empty=%d\n", link_empty(S) ? 1 : 0);
    return 1;
}

static int size_link(LinkStack *S) {
    printf("ok size=%d stack=", link_size(S));
    link_print(S, stdout);
    printf("\n");
    return 1;
}

static int dispatch_link(LinkStack *S, const char *cmd) {
    if (strcmp(cmd, "push") == 0) {
        return push_link(S);
    }
    if (strcmp(cmd, "pop") == 0) {
        return pop_link(S);
    }
    if (strcmp(cmd, "top") == 0) {
        return top_link(S);
    }
    if (strcmp(cmd, "empty") == 0) {
        return empty_link(S);
    }
    if (strcmp(cmd, "size") == 0) {
        return size_link(S);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

static int run_link(void) {
    LinkStack stack;
    char cmd[16];
    if (!link_init(&stack)) {
        return 1;
    }
    stack.trace = stdout;
    printf("kind=link top=NULL empty_when=top==NULL\n");
    for (;;) {
        prompt("请输入命令（push e / pop / top / empty / size，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            break;
        }
        if (!dispatch_link(&stack, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            link_destroy(&stack);
            return 1;
        }
    }
    link_destroy(&stack);
    return 0;
}

int main(void) {
    char kind[16];
    prompt("请输入栈的种类（sq 或 link）：");
    if (scanf("%15s", kind) != 1) {
        return 0;
    }
    if (strcmp(kind, "sq") == 0) {
        return run_sq();
    }
    if (strcmp(kind, "link") == 0) {
        return run_link();
    }
    printf("fail kind %s\n", kind);
    return 0;
}
