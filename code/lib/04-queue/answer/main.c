/*
 * 牺牲一个单元的循环队列。数组长度 8，最多放 7 个元素。
 * front 指向队头元素，rear 指向下一空位。
 */
#include <stdio.h>
#include <string.h>

#include "cycq.h"
#include "prompt.h"

static void print_formula(const CycQ *Q) {
    printf("(%d-%d+%d)%%%d=%d", Q->rear, Q->front, Q->cap, Q->cap, cyc_count(Q));
}

static void print_ok(const char *verb, int e, int has_e, const CycQ *Q) {
    if (has_e) {
        printf("ok %s e=%d count=%d formula=", verb, e, cyc_count(Q));
    } else {
        printf("ok %s count=%d formula=", verb, cyc_count(Q));
    }
    print_formula(Q);
    printf("\n");
}

static int cmd_enq(CycQ *Q) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!cyc_enq(Q, e)) {
        printf("fail enq full e=%d front=%d rear=%d cond=(rear+1)%%%d==front\n", e, Q->front, Q->rear,
               Q->cap);
        return 1;
    }
    print_ok("enq", e, 1, Q);
    return 1;
}

static int cmd_deq(CycQ *Q) {
    int e = 0;
    if (!cyc_deq(Q, &e)) {
        printf("fail deq empty front=%d rear=%d cond=front==rear\n", Q->front, Q->rear);
        return 1;
    }
    print_ok("deq", e, 1, Q);
    return 1;
}

static int cmd_head(CycQ *Q) {
    int e = 0;
    if (!cyc_head(Q, &e)) {
        printf("fail head empty\n");
        return 1;
    }
    printf("ok head e=%d front=%d\n", e, Q->front);
    return 1;
}

static int cmd_tail(CycQ *Q) {
    int e = 0;
    int index = 0;
    if (!cyc_tail(Q, &e, &index)) {
        printf("fail tail empty\n");
        return 1;
    }
    printf("ok tail e=%d index=%d rear=%d\n", e, index, Q->rear);
    return 1;
}

static int cmd_empty(CycQ *Q) {
    printf("ok empty=%d front=%d rear=%d cond=front==rear\n", cyc_empty(Q) ? 1 : 0, Q->front, Q->rear);
    return 1;
}

static int cmd_size(CycQ *Q) {
    printf("ok size=%d front=%d rear=%d formula=", cyc_count(Q), Q->front, Q->rear);
    print_formula(Q);
    printf("\n");
    return 1;
}

static int cmd_state(CycQ *Q) {
    printf("state front=%d rear=%d count=%d full=%d empty=%d formula=", Q->front, Q->rear, cyc_count(Q),
           cyc_full(Q) ? 1 : 0, cyc_empty(Q) ? 1 : 0);
    print_formula(Q);
    printf(" cells=");
    cyc_print_cells(Q, stdout);
    printf("\n");
    return 1;
}

static int dispatch(CycQ *Q, const char *cmd) {
    if (strcmp(cmd, "enq") == 0) {
        return cmd_enq(Q);
    }
    if (strcmp(cmd, "deq") == 0) {
        return cmd_deq(Q);
    }
    if (strcmp(cmd, "head") == 0) {
        return cmd_head(Q);
    }
    if (strcmp(cmd, "tail") == 0) {
        return cmd_tail(Q);
    }
    if (strcmp(cmd, "empty") == 0) {
        return cmd_empty(Q);
    }
    if (strcmp(cmd, "size") == 0) {
        return cmd_size(Q);
    }
    if (strcmp(cmd, "state") == 0) {
        return cmd_state(Q);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

static void print_init(const CycQ *Q) {
    printf("init front=0 rear=0 cap=%d max=%d empty_when=front==rear full_when=(rear+1)%%%d==front cells=",
           Q->cap, Q->cap - 1, Q->cap);
    cyc_print_cells(Q, stdout);
    printf("\n");
}

int main(void) {
    CycQ queue;
    char cmd[16];
    if (!cyc_init(&queue, CYC_DEMO_CAP)) {
        return 1;
    }
    queue.trace = stdout;
    print_init(&queue);
    for (;;) {
        prompt("请输入命令（enq e / deq / head / tail / empty / size / state，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(&queue, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
