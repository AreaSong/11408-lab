#include "soft.h"

#include "rule.h"

#include <string.h>

void soft_init(Soft *s) {
    memset(s, 0, sizeof *s);
}

static int who_ok(int who) {
    return who == 0 || who == 1;
}

static const char *inside_of(const Soft *s) {
    if (s->in_cs[0] && s->in_cs[1]) {
        return "both";
    }
    if (s->in_cs[0]) {
        return "0";
    }
    if (s->in_cs[1]) {
        return "1";
    }
    return "-";
}

static void fill(const Soft *s, SoftView *v, const char *action) {
    v->action = action;
    v->turn = s->turn;
    v->flag0 = s->flag[0];
    v->flag1 = s->flag[1];
    v->lock = s->lock;
    v->inside = inside_of(s);
}

static int reject_in(const Soft *s, int who) {
    if (!who_ok(who)) {
        return -1;
    }
    if (s->in_cs[who]) {
        return -3;
    }
    return 0;
}

static int begin_leave(Soft *s, int who) {
    if (!who_ok(who)) {
        return -1;
    }
    if (!s->in_cs[who]) {
        return -2;
    }
    s->in_cs[who] = 0;
    s->pc[who] = 0;
    return 0;
}

static int leave_single(Soft *s, int who, SoftView *v) {
    int rc = begin_leave(s, who);
    if (rc != 0) {
        return rc;
    }
    /* 退出区把轮次交给对方。对方若不再进来，自己下一轮会在空临界区外面忙等。 */
    s->turn = 1 - who;
    fill(s, v, "leave");
    return 0;
}

static int try_single(Soft *s, int who, SoftView *v) {
    int rc = reject_in(s, who);
    if (rc != 0) {
        return rc;
    }
    if (s->turn != who) {
        fill(s, v, "spin");
        return 0;
    }
    s->in_cs[who] = 1;
    fill(s, v, "enter");
    return 0;
}

static int leave_flag(Soft *s, int who, SoftView *v) {
    int rc = begin_leave(s, who);
    if (rc != 0) {
        return rc;
    }
    s->flag[who] = 0;
    fill(s, v, "leave");
    return 0;
}

static int try_check_first(Soft *s, int who, SoftView *v) {
    int other = 1 - who;
    int rc = reject_in(s, who);
    if (rc != 0) {
        return rc;
    }
    if (s->pc[who] == 0) {
        if (s->flag[other]) {
            fill(s, v, "spin");
            return 0;
        }
        /* 先看到对方没进来，下一行才置自己的标志。两行之间可以被切走。 */
        s->pc[who] = 1;
        fill(s, v, "pass");
        return 0;
    }
    s->flag[who] = 1;
    s->in_cs[who] = 1;
    s->pc[who] = 2;
    fill(s, v, "enter");
    return 0;
}

static int try_check_after(Soft *s, int who, SoftView *v) {
    int other = 1 - who;
    int rc = reject_in(s, who);
    if (rc != 0) {
        return rc;
    }
    if (s->pc[who] == 0) {
        s->flag[who] = 1;
        s->pc[who] = 1;
        fill(s, v, "flag");
        return 0;
    }
    if (s->flag[other]) {
        fill(s, v, "spin");
        return 0;
    }
    s->in_cs[who] = 1;
    s->pc[who] = 2;
    fill(s, v, "enter");
    return 0;
}

static int try_peterson(Soft *s, int who, SoftView *v) {
    int other = 1 - who;
    int rc = reject_in(s, who);
    if (rc != 0) {
        return rc;
    }
    if (s->pc[who] == 0) {
        s->flag[who] = 1;
        s->pc[who] = 1;
        fill(s, v, "flag");
        return 0;
    }
    if (s->pc[who] == 1) {
        /* turn 写对方：最后写入的人谦让，先等。两边不可能同时觉得轮到自己。 */
        s->turn = other;
        s->pc[who] = 2;
        fill(s, v, "yield");
        return 0;
    }
    if (s->flag[other] && s->turn == other) {
        fill(s, v, "spin");
        return 0;
    }
    s->in_cs[who] = 1;
    s->pc[who] = 3;
    fill(s, v, "enter");
    return 0;
}

static int leave_lock(Soft *s, int who, SoftView *v) {
    int rc = begin_leave(s, who);
    if (rc != 0) {
        return rc;
    }
    s->lock = 0;
    fill(s, v, "leave");
    return 0;
}

static int try_tsl(Soft *s, int who, SoftView *v) {
    int old;
    int rc = reject_in(s, who);
    if (rc != 0) {
        return rc;
    }
    /* 读旧值并写成 1 是一条指令。软件方法里「检查」和「上锁」能被从中间切开，这里不能。 */
    old = s->lock;
    s->lock = 1;
    if (old) {
        fill(s, v, "spin");
        return 0;
    }
    s->in_cs[who] = 1;
    fill(s, v, "enter");
    return 0;
}

static int try_swap(Soft *s, int who, SoftView *v) {
    int key = 1;
    int old;
    int rc = reject_in(s, who);
    if (rc != 0) {
        return rc;
    }
    /* key 先置 true，再和 lock 交换。换回来仍是 true，说明锁原来就被人占着。 */
    old = s->lock;
    s->lock = key;
    key = old;
    if (key) {
        fill(s, v, "spin");
        return 0;
    }
    s->in_cs[who] = 1;
    fill(s, v, "enter");
    return 0;
}

int soft_step(Soft *s, int algo, int who, int leave, SoftView *view) {
    if (!rule_can_step(algo)) {
        return -4;
    }
    if (leave) {
        if (algo == ALGO_SINGLE) {
            return leave_single(s, who, view);
        }
        if (algo == ALGO_TSL || algo == ALGO_SWAP) {
            return leave_lock(s, who, view);
        }
        return leave_flag(s, who, view);
    }
    switch (algo) {
    case ALGO_SINGLE:
        return try_single(s, who, view);
    case ALGO_CHECK_FIRST:
        return try_check_first(s, who, view);
    case ALGO_CHECK_AFTER:
        return try_check_after(s, who, view);
    case ALGO_PETERSON:
        return try_peterson(s, who, view);
    case ALGO_TSL:
        return try_tsl(s, who, view);
    default:
        return try_swap(s, who, view);
    }
}
