/*
 * 元素个数。slot：rear 指向下一空位，个数 (rear - front + cap) % cap。
 * elem / init：2011-3，rear 指向队尾元素，入队先移动 rear 再写入。
 */
#include <stdio.h>
#include <string.h>

#include "cycq.h"
#include "prompt.h"
#include "tailq.h"

static int pointers_ok(int cap, int front, int rear) {
    return front >= 0 && rear >= 0 && front < cap && rear < cap;
}

static int cmd_slot(void) {
    int cap = 0;
    int front = 0;
    int rear = 0;
    int count;
    int empty;
    int full;
    if (scanf("%d %d %d", &cap, &front, &rear) != 3) {
        return 0;
    }
    if (cap < 2 || cap > CYC_MAX || !pointers_ok(cap, front, rear)) {
        printf("fail cap=%d front=%d rear=%d\n", cap, front, rear);
        return 1;
    }
    count = cyc_length(cap, front, rear);
    empty = front == rear;
    full = (rear + 1) % cap == front;
    printf("case slot cap=%d front=%d rear=%d\n", cap, front, rear);
    printf("count=(%d-%d+%d)%%%d=%d empty=%d full=%d\n", rear, front, cap, cap, count, empty, full);
    return 1;
}

static int cmd_elem(void) {
    int cap = 0;
    int front = 0;
    int rear = 0;
    int empty;
    int full;
    int count;
    if (scanf("%d %d %d", &cap, &front, &rear) != 3) {
        return 0;
    }
    if (cap < 2 || cap > TAIL_MAX || !pointers_ok(cap, front, rear)) {
        printf("fail cap=%d front=%d rear=%d\n", cap, front, rear);
        return 1;
    }
    empty = (rear + 1) % cap == front;
    full = (rear + 2) % cap == front;
    count = empty ? 0 : (rear - front + cap) % cap + 1;
    printf("case elem cap=%d front=%d rear=%d\n", cap, front, rear);
    if (empty) {
        printf("empty=1 count=0 full=%d\n", full);
    } else {
        printf("empty=0 count=(%d-%d+%d)%%%d+1=%d full=%d\n", rear, front, cap, cap, count, full);
    }
    return 1;
}

static int cmd_init(void) {
    TailQ queue;
    int n = 0;
    int old_rear;
    if (scanf("%d", &n) != 1) {
        return 0;
    }
    if (!tail_init(&queue, n)) {
        printf("fail n=%d\n", n);
        return 1;
    }
    old_rear = queue.rear;
    printf("case init n=%d\n", n);
    printf("front=%d rear=%d empty=%d\n", queue.front, queue.rear, tail_empty(&queue) ? 1 : 0);
    if (!tail_enq(&queue, 1)) {
        printf("fail enq full\n");
        return 1;
    }
    printf("enq e=1 rear=%d->%d index=%d front=%d empty=%d count=%d\n", old_rear, queue.rear, queue.rear,
           queue.front, tail_empty(&queue) ? 1 : 0, tail_count(&queue));
    return 1;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "slot") == 0) {
        return cmd_slot();
    }
    if (strcmp(cmd, "elem") == 0) {
        return cmd_elem();
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init();
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[16];
    for (;;) {
        prompt("请输入命令（slot cap front rear / elem cap front rear / init n，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
