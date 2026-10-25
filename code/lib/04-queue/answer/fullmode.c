/*
 * 同一串操作分别跑在三种判满的循环队列上。
 * 数组长度相同。牺牲单元最多放 cap-1 个，size 和 tag 可以放满 cap 个。
 */
#include <stdio.h>
#include <string.h>

#include "cycq.h"
#include "prompt.h"
#include "sizeq.h"
#include "tagq.h"

static void enq_sac(CycQ *Q, int e) {
    int old_front = Q->front;
    int old_rear = Q->rear;
    if (!cyc_enq(Q, e)) {
        printf("sacrifice fail full front=%d rear=%d\n", Q->front, Q->rear);
        return;
    }
    printf("sacrifice front=%d->%d rear=%d->%d count=%d full=%d\n", old_front, Q->front, old_rear, Q->rear,
           cyc_count(Q), cyc_full(Q) ? 1 : 0);
}

static void enq_size(SizeQ *Q, int e) {
    int old_front = Q->front;
    int old_rear = Q->rear;
    if (!size_enq(Q, e)) {
        printf("size fail full front=%d rear=%d count=%d\n", Q->front, Q->rear, size_count(Q));
        return;
    }
    printf("size front=%d->%d rear=%d->%d count=%d full=%d\n", old_front, Q->front, old_rear, Q->rear,
           size_count(Q), size_full(Q) ? 1 : 0);
}

static void enq_tag(TagQ *Q, int e) {
    int old_front = Q->front;
    int old_rear = Q->rear;
    int old_tag = Q->tag;
    if (!tag_enq(Q, e)) {
        printf("tag fail full front=%d rear=%d tag=%d\n", Q->front, Q->rear, Q->tag);
        return;
    }
    printf("tag front=%d->%d rear=%d->%d tag=%d->%d count=%d full=%d raw=%d\n", old_front, Q->front, old_rear,
           Q->rear, old_tag, Q->tag, tag_count(Q), tag_full(Q) ? 1 : 0, tag_raw(Q));
}

static int cmd_enq(CycQ *c, SizeQ *s, TagQ *t) {
    int e = 0;
    if (scanf("%d", &e) != 1) {
        return 0;
    }
    printf("enq e=%d\n", e);
    enq_sac(c, e);
    enq_size(s, e);
    enq_tag(t, e);
    return 1;
}

static void deq_sac(CycQ *Q) {
    int old_front = Q->front;
    int old_rear = Q->rear;
    int e = 0;
    if (!cyc_deq(Q, &e)) {
        printf("sacrifice fail empty front=%d rear=%d\n", Q->front, Q->rear);
        return;
    }
    printf("sacrifice e=%d front=%d->%d rear=%d->%d count=%d full=%d\n", e, old_front, Q->front, old_rear,
           Q->rear, cyc_count(Q), cyc_full(Q) ? 1 : 0);
}

static void deq_size(SizeQ *Q) {
    int old_front = Q->front;
    int old_rear = Q->rear;
    int e = 0;
    if (!size_deq(Q, &e)) {
        printf("size fail empty front=%d rear=%d count=%d\n", Q->front, Q->rear, size_count(Q));
        return;
    }
    printf("size e=%d front=%d->%d rear=%d->%d count=%d full=%d\n", e, old_front, Q->front, old_rear, Q->rear,
           size_count(Q), size_full(Q) ? 1 : 0);
}

static void deq_tag(TagQ *Q) {
    int old_front = Q->front;
    int old_rear = Q->rear;
    int old_tag = Q->tag;
    int e = 0;
    if (!tag_deq(Q, &e)) {
        printf("tag fail empty front=%d rear=%d tag=%d\n", Q->front, Q->rear, Q->tag);
        return;
    }
    printf("tag e=%d front=%d->%d rear=%d->%d tag=%d->%d count=%d full=%d raw=%d\n", e, old_front, Q->front,
           old_rear, Q->rear, old_tag, Q->tag, tag_count(Q), tag_full(Q) ? 1 : 0, tag_raw(Q));
}

static int cmd_deq(CycQ *c, SizeQ *s, TagQ *t) {
    printf("deq\n");
    deq_sac(c);
    deq_size(s);
    deq_tag(t);
    return 1;
}

static int cmd_state(const CycQ *c, const SizeQ *s, const TagQ *t) {
    printf("state\n");
    printf("sacrifice front=%d rear=%d count=%d full=%d empty=%d cells=", c->front, c->rear, cyc_count(c),
           cyc_full(c) ? 1 : 0, cyc_empty(c) ? 1 : 0);
    cyc_print_cells(c, stdout);
    printf("\n");
    printf("size front=%d rear=%d count=%d full=%d empty=%d cells=", s->front, s->rear, size_count(s),
           size_full(s) ? 1 : 0, size_empty(s) ? 1 : 0);
    size_print_cells(s, stdout);
    printf("\n");
    printf("tag front=%d rear=%d tag=%d count=%d full=%d empty=%d raw=%d cells=", t->front, t->rear, t->tag,
           tag_count(t), tag_full(t) ? 1 : 0, tag_empty(t) ? 1 : 0, tag_raw(t));
    tag_print_cells(t, stdout);
    printf("\n");
    return 1;
}

static int dispatch(CycQ *c, SizeQ *s, TagQ *t, const char *cmd) {
    if (strcmp(cmd, "enq") == 0) {
        return cmd_enq(c, s, t);
    }
    if (strcmp(cmd, "deq") == 0) {
        return cmd_deq(c, s, t);
    }
    if (strcmp(cmd, "state") == 0) {
        return cmd_state(c, s, t);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

static int open_all(CycQ *c, SizeQ *s, TagQ *t, int cap) {
    if (cap < 2 || cap > 16 || !cyc_init(c, cap) || !size_init(s, cap) || !tag_init(t, cap)) {
        printf("fail cap=%d\n", cap);
        return 0;
    }
    printf("init cap=%d\n", cap);
    printf("sacrifice max=%d empty=front==rear full=(rear+1)%%%d==front\n", cap - 1, cap);
    printf("size max=%d empty=count==0 full=count==%d\n", cap, cap);
    printf("tag max=%d empty=front==rear&&tag==0 full=front==rear&&tag==1\n", cap);
    return 1;
}

int main(void) {
    CycQ cyc;
    SizeQ sized;
    TagQ tagged;
    char cmd[16];
    int cap = 0;
    prompt("请输入数组长度（2 到 16）：");
    if (scanf("%d", &cap) != 1) {
        return 0;
    }
    if (!open_all(&cyc, &sized, &tagged, cap)) {
        return 0;
    }
    for (;;) {
        prompt("请输入命令（enq e / deq / state，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(&cyc, &sized, &tagged, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
