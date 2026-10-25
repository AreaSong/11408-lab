/*
 * 两端都能入队、出队的循环队列，牺牲一个单元。
 * judge 对照 2014-3 的四个判空、判满条件。
 */
#include <stdio.h>
#include <string.h>

#include "ends.h"
#include "prompt.h"

static int same(int empty, int full, int real_empty, int real_full) {
    return empty == real_empty && full == real_full;
}

static void print_opt(const char *name, int empty, int full, const EndsJudge *j) {
    printf("%s empty=%d full=%d match=%d\n", name, empty, full,
           same(empty, full, j->real_empty, j->real_full));
}

static int cmd_judge(void) {
    EndsJudge judge;
    int cap = 0;
    int end1 = 0;
    int end2 = 0;
    int count;
    if (scanf("%d %d %d", &cap, &end1, &end2) != 3) {
        return 0;
    }
    if (!ends_judge(cap, end1, end2, &judge)) {
        printf("fail cap=%d end1=%d end2=%d\n", cap, end1, end2);
        return 1;
    }
    count = judge.real_empty ? 0 : (end2 - end1 + cap) % cap;
    printf("case judge cap=%d end1=%d end2=%d\n", cap, end1, end2);
    printf("real empty=%d full=%d count=%d\n", judge.real_empty, judge.real_full, count);
    print_opt("A", judge.a_empty, judge.a_full, &judge);
    print_opt("B", judge.b_empty, judge.b_full, &judge);
    print_opt("C", judge.c_empty, judge.c_full, &judge);
    print_opt("D", judge.d_empty, judge.d_full, &judge);
    return 1;
}

static void print_init(const EndsQ *Q) {
    printf("init front=0 rear=0 cap=%d max=%d empty_when=front==rear full_when=(rear+1)%%%d==front cells=",
           Q->cap, Q->cap - 1, Q->cap);
    ends_print_cells(Q, stdout);
    printf("\n");
}

static int cmd_ops(EndsQ *Q, int *ready) {
    EndsQ fresh;
    int cap = 0;
    if (scanf("%d", &cap) != 1) {
        return 0;
    }
    if (!ends_init(&fresh, cap)) {
        printf("fail cap=%d\n", cap);
        return 1;
    }
    *Q = fresh;
    Q->trace = stdout;
    *ready = 1;
    print_init(Q);
    return 1;
}

static int need_queue(int ready) {
    if (ready) {
        return 1;
    }
    printf("fail noqueue\n");
    return 0;
}

static int cmd_enq(EndsQ *Q, int ready, int front) {
    int e = 0;
    int ok;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    if (!need_queue(ready)) {
        return 1;
    }
    ok = front ? ends_enq_front(Q, e) : ends_enq_rear(Q, e);
    if (!ok) {
        printf("fail %s full front=%d rear=%d cond=(rear+1)%%%d==front\n", front ? "enq-f" : "enq-r",
               Q->front, Q->rear, Q->cap);
        return 1;
    }
    printf("ok %s e=%d count=%d\n", front ? "enq-f" : "enq-r", e, ends_count(Q));
    return 1;
}

static int cmd_deq(EndsQ *Q, int ready, int front) {
    int e = 0;
    int ok;
    if (!need_queue(ready)) {
        return 1;
    }
    ok = front ? ends_deq_front(Q, &e) : ends_deq_rear(Q, &e);
    if (!ok) {
        printf("fail %s empty front=%d rear=%d cond=front==rear\n", front ? "deq-f" : "deq-r", Q->front,
               Q->rear);
        return 1;
    }
    printf("ok %s e=%d count=%d\n", front ? "deq-f" : "deq-r", e, ends_count(Q));
    return 1;
}

static int cmd_state(const EndsQ *Q, int ready) {
    if (!need_queue(ready)) {
        return 1;
    }
    printf("state front=%d rear=%d count=%d full=%d empty=%d formula=(%d-%d+%d)%%%d=%d cells=", Q->front,
           Q->rear, ends_count(Q), ends_full(Q) ? 1 : 0, ends_empty(Q) ? 1 : 0, Q->rear, Q->front, Q->cap,
           Q->cap, ends_count(Q));
    ends_print_cells(Q, stdout);
    printf("\n");
    return 1;
}

static int dispatch(EndsQ *Q, int *ready, const char *cmd) {
    if (strcmp(cmd, "judge") == 0) {
        return cmd_judge();
    }
    if (strcmp(cmd, "ops") == 0) {
        return cmd_ops(Q, ready);
    }
    if (strcmp(cmd, "enq-f") == 0) {
        return cmd_enq(Q, *ready, 1);
    }
    if (strcmp(cmd, "enq-r") == 0) {
        return cmd_enq(Q, *ready, 0);
    }
    if (strcmp(cmd, "deq-f") == 0) {
        return cmd_deq(Q, *ready, 1);
    }
    if (strcmp(cmd, "deq-r") == 0) {
        return cmd_deq(Q, *ready, 0);
    }
    if (strcmp(cmd, "state") == 0) {
        return cmd_state(Q, *ready);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    EndsQ queue;
    char cmd[16];
    int ready = 0;
    for (;;) {
        prompt("请输入命令（judge cap end1 end2 / ops cap / enq-f e / enq-r e / deq-f / deq-r / state，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(&queue, &ready, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
