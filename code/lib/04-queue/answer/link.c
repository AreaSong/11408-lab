/*
 * 带头结点的链队列。空队列时 front 和 rear 都指向头结点。
 */
#include <stdio.h>
#include <string.h>

#include "linkq.h"
#include "prompt.h"

static void print_ok(const char *verb, int e, int has_e, const LinkQ *Q) {
    if (has_e) {
        printf("ok %s e=%d count=%d q=", verb, e, link_count(Q));
    } else {
        printf("ok %s count=%d q=", verb, link_count(Q));
    }
    link_print(Q, stdout);
    printf("\n");
}

static int cmd_enq(LinkQ *Q) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!link_enq(Q, e)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    print_ok("enq", e, 1, Q);
    return 1;
}

static int cmd_deq(LinkQ *Q) {
    int e = 0;
    if (!link_deq(Q, &e)) {
        printf("fail deq empty cond=front==rear\n");
        return 1;
    }
    print_ok("deq", e, 1, Q);
    return 1;
}

static int cmd_head(LinkQ *Q) {
    int e = 0;
    if (!link_head(Q, &e)) {
        printf("fail head empty\n");
        return 1;
    }
    printf("ok head e=%d\n", e);
    return 1;
}

static int cmd_empty(LinkQ *Q) {
    printf("ok empty=%d cond=front==rear\n", link_empty(Q) ? 1 : 0);
    return 1;
}

static int cmd_size(LinkQ *Q) {
    printf("ok size=%d q=", link_count(Q));
    link_print(Q, stdout);
    printf("\n");
    return 1;
}

static int dispatch(LinkQ *Q, const char *cmd) {
    if (strcmp(cmd, "enq") == 0) {
        return cmd_enq(Q);
    }
    if (strcmp(cmd, "deq") == 0) {
        return cmd_deq(Q);
    }
    if (strcmp(cmd, "head") == 0) {
        return cmd_head(Q);
    }
    if (strcmp(cmd, "empty") == 0) {
        return cmd_empty(Q);
    }
    if (strcmp(cmd, "size") == 0) {
        return cmd_size(Q);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    LinkQ queue;
    char cmd[16];
    if (!link_init(&queue)) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    queue.trace = stdout;
    printf("init front==rear head next=NULL empty_when=front==rear\n");
    for (;;) {
        prompt("请输入命令（enq e / deq / head / empty / size，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            break;
        }
        if (!dispatch(&queue, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            link_destroy(&queue);
            return 1;
        }
    }
    link_destroy(&queue);
    return 0;
}
