#include "kernel/k_pv.h"

#include <string.h>

#define PV_ACT 6
#define PV_SEM 8
#define PV_OP 24

enum { OP_P = 1, OP_V = 2, OP_WORK = 3, OP_END = 4 };

typedef struct {
    int op;
    int sem;
    int tag;
} PvOp;

typedef struct {
    PvOp op[PV_OP];
    int n;
    int pc;
    int blocked;
    int done;
} PvAct;

typedef struct {
    int value;
    int q[PV_ACT];
    int nq;
    int hold[PV_ACT];
} PvSem;

typedef struct {
    PvSem sem[PV_SEM];
    PvAct act[PV_ACT];
    int nact;
    int cursor;
    int produced;
    int consumed;
    int meals;
    int reading;
    int max_read;
    int deadlock;
    int cycle;
} PvWorld;

static void add_op(PvAct *a, int op, int sem, int tag) {
    if (a->n < PV_OP) {
        a->op[a->n].op = op;
        a->op[a->n].sem = sem;
        a->op[a->n].tag = tag;
        a->n++;
    }
}

static void wake_one(PvWorld *w, PvSem *s) {
    int who;
    int i;
    if (s->nq <= 0) {
        return;
    }
    who = s->q[0];
    for (i = 1; i < s->nq; i++) {
        s->q[i - 1] = s->q[i];
    }
    s->nq--;
    w->act[who].blocked = -1;
    s->hold[who]++;
}

static void do_p(PvWorld *w, int who, int sem) {
    PvAct *a = &w->act[who];
    PvSem *s = &w->sem[sem];
    s->value--;
    a->pc++;
    if (s->value < 0) {
        s->q[s->nq++] = who;
        a->blocked = sem;
        return;
    }
    s->hold[who]++;
}

static void do_v(PvWorld *w, int who, int sem) {
    PvSem *s = &w->sem[sem];
    s->value++;
    if (s->hold[who] > 0) {
        s->hold[who]--;
    }
    if (s->value <= 0) {
        wake_one(w, s);
    }
    w->act[who].pc++;
}

static void do_work(PvWorld *w, int who) {
    int tag = w->act[who].op[w->act[who].pc].tag;
    if (tag == 1) {
        w->produced++;
    } else if (tag == 2) {
        w->consumed++;
    } else if (tag == 3) {
        w->meals++;
    } else if (tag == 4) {
        w->reading++;
        if (w->reading > w->max_read) {
            w->max_read = w->reading;
        }
    } else if (tag == 5 && w->reading > 0) {
        w->reading--;
    }
    w->act[who].pc++;
}

static void exec_one(PvWorld *w, int who) {
    PvAct *a = &w->act[who];
    PvOp op = a->op[a->pc];
    if (op.op == OP_P) {
        do_p(w, who, op.sem);
    } else if (op.op == OP_V) {
        do_v(w, who, op.sem);
    } else if (op.op == OP_WORK) {
        do_work(w, who);
    } else {
        a->done = 1;
        a->pc++;
    }
}

static int all_done(const PvWorld *w) {
    int i;
    for (i = 0; i < w->nact; i++) {
        if (!w->act[i].done) {
            return 0;
        }
    }
    return 1;
}

static int color_walk(int u, int adj[][PV_ACT], const int *len, int *color) {
    int i;
    color[u] = 1;
    for (i = 0; i < len[u]; i++) {
        int v = adj[u][i];
        if (color[v] == 1 || (color[v] == 0 && color_walk(v, adj, len, color))) {
            return 1;
        }
    }
    color[u] = 2;
    return 0;
}

static void see_cycle(PvWorld *w) {
    int adj[PV_ACT][PV_ACT];
    int len[PV_ACT];
    int color[PV_ACT];
    int i;
    int h;
    memset(adj, 0, sizeof adj);
    memset(len, 0, sizeof len);
    memset(color, 0, sizeof color);
    for (i = 0; i < w->nact; i++) {
        int sem;
        int holders = 0;
        if (w->act[i].blocked < 0) {
            continue;
        }
        sem = w->act[i].blocked;
        for (h = 0; h < w->nact; h++) {
            if (w->sem[sem].hold[h] > 0 && len[i] < PV_ACT) {
                adj[i][len[i]++] = h;
                holders++;
            }
        }
        if (holders == 0) {
            for (h = 0; h < w->nact; h++) {
                if (h != i && !w->act[h].done && len[i] < PV_ACT) {
                    adj[i][len[i]++] = h;
                }
            }
        }
    }
    for (i = 0; i < w->nact; i++) {
        if (color[i] == 0 && color_walk(i, adj, len, color)) {
            w->cycle = 1;
            return;
        }
    }
}

static int run_world(PvWorld *w) {
    int step;
    for (step = 0; step < 400; step++) {
        int k;
        int ran = 0;
        for (k = 0; k < w->nact; k++) {
            int who = (w->cursor + k) % w->nact;
            if (w->act[who].done || w->act[who].blocked >= 0 || w->act[who].pc >= w->act[who].n) {
                continue;
            }
            exec_one(w, who);
            w->cursor = (who + 1) % w->nact;
            ran = 1;
            break;
        }
        if (!ran) {
            w->deadlock = !all_done(w);
            if (w->deadlock) {
                see_cycle(w);
            }
            return 0;
        }
    }
    w->deadlock = 1;
    return 0;
}

static void zero_world(PvWorld *w) {
    int i;
    memset(w, 0, sizeof *w);
    for (i = 0; i < PV_ACT; i++) {
        w->act[i].blocked = -1;
    }
}

static void pair_loop(PvAct *a, int first, int second, int v1, int v2, int tag, int times) {
    int n;
    for (n = 0; n < times; n++) {
        add_op(a, OP_P, first, 0);
        add_op(a, OP_P, second, 0);
        add_op(a, OP_WORK, 0, tag);
        add_op(a, OP_V, v1, 0);
        add_op(a, OP_V, v2, 0);
    }
    add_op(a, OP_END, 0, 0);
}

static int run_pc(int wrong, KPvOut *out) {
    PvWorld w;
    zero_world(&w);
    w.nact = 2;
    w.sem[0].value = 1;
    w.sem[1].value = 0;
    w.sem[2].value = 1;
    if (wrong) {
        pair_loop(&w.act[0], 2, 1, 1, 2, 2, 2);
        pair_loop(&w.act[1], 2, 0, 0, 2, 1, 2);
    } else {
        pair_loop(&w.act[0], 1, 2, 2, 0, 2, 2);
        pair_loop(&w.act[1], 0, 2, 2, 1, 1, 2);
    }
    run_world(&w);
    out->deadlock = w.deadlock;
    out->produced = w.produced;
    out->consumed = w.consumed;
    out->cycle = w.cycle;
    return 0;
}

static int run_phi(int room, KPvOut *out) {
    PvWorld w;
    int i;
    zero_world(&w);
    w.nact = 5;
    for (i = 0; i < 5; i++) {
        w.sem[i].value = 1;
        if (room) {
            add_op(&w.act[i], OP_P, 5, 0);
        }
        add_op(&w.act[i], OP_P, i, 0);
        add_op(&w.act[i], OP_P, (i + 1) % 5, 0);
        add_op(&w.act[i], OP_WORK, 0, 3);
        add_op(&w.act[i], OP_V, (i + 1) % 5, 0);
        add_op(&w.act[i], OP_V, i, 0);
        if (room) {
            add_op(&w.act[i], OP_V, 5, 0);
        }
        add_op(&w.act[i], OP_END, 0, 0);
    }
    w.sem[5].value = 4;
    run_world(&w);
    out->deadlock = w.deadlock;
    out->meals = w.meals;
    out->cycle = w.cycle;
    return 0;
}

/* P 减完就挂起。阶段号先往前走，醒来从下一段接着做，不能再减一次。 */
static int reader_in(PvWorld *w, int who, int *rc, int *phase) {
    int st = phase[who];
    if (st == 0) {
        do_p(w, who, 0);
        phase[who] = 1;
        return 1;
    }
    if (st == 1) {
        (*rc)++;
        phase[who] = (*rc == 1) ? 2 : 3;
        return 1;
    }
    if (st == 2) {
        do_p(w, who, 1);
        phase[who] = 3;
        return 1;
    }
    if (st == 3) {
        do_v(w, who, 0);
        phase[who] = 4;
        return 1;
    }
    w->reading++;
    if (w->reading > w->max_read) {
        w->max_read = w->reading;
    }
    phase[who] = 5;
    return 1;
}

static int reader_out(PvWorld *w, int who, int *rc, int *phase) {
    int st = phase[who];
    if (st == 5) {
        if (w->reading > 0) {
            w->reading--;
        }
        phase[who] = 6;
        return 1;
    }
    if (st == 6) {
        do_p(w, who, 0);
        phase[who] = 7;
        return 1;
    }
    if (st == 7) {
        int last = (*rc == 1);
        (*rc)--;
        phase[who] = last ? 8 : 9;
        return 1;
    }
    if (st == 8) {
        do_v(w, who, 1);
        phase[who] = 9;
        return 1;
    }
    do_v(w, who, 0);
    phase[who] = 10;
    w->act[who].done = 1;
    return 1;
}

static int reader_step(PvWorld *w, int who, int *rc, int *phase) {
    if (phase[who] <= 4) {
        return reader_in(w, who, rc, phase);
    }
    return reader_out(w, who, rc, phase);
}

static int writer_step(PvWorld *w, int *phase) {
    int st = *phase;
    if (st == 0) {
        do_p(w, 2, 1);
        *phase = 1;
        return 1;
    }
    if (st == 1) {
        if (w->reading != 0) {
            w->deadlock = 1;
        }
        *phase = 2;
        return 1;
    }
    do_v(w, 2, 1);
    *phase = 3;
    w->act[2].done = 1;
    return 1;
}

static int runnable_reader(const PvWorld *w, int who, const int *phase) {
    return who < 2 && !w->act[who].done && w->act[who].blocked < 0 && phase[who] < 10;
}

static int run_read(KPvOut *out) {
    PvWorld w;
    int phase[2] = {0, 0};
    int wp = 0;
    int rc = 0;
    int step;
    int turn = 0;
    zero_world(&w);
    w.nact = 3;
    w.sem[0].value = 1;
    w.sem[1].value = 1;
    for (step = 0; step < 80; step++) {
        int before;
        int k;
        int picked = -1;
        for (k = 0; k < 3; k++) {
            int who = (turn + k) % 3;
            if (who < 2 && runnable_reader(&w, who, phase)) {
                picked = who;
                break;
            }
            if (who == 2 && !w.act[2].done && w.act[2].blocked < 0 && wp < 3) {
                picked = 2;
                break;
            }
        }
        if (picked < 0) {
            break;
        }
        if (picked == 2) {
            writer_step(&w, &wp);
            turn = 0;
            continue;
        }
        /* 进了读区就让给另一个读者，这样两个人能同时在里面。 */
        before = w.reading;
        reader_step(&w, picked, &rc, phase);
        if (w.act[picked].blocked >= 0 || w.reading > before || w.act[picked].done) {
            turn = (picked + 1) % 3;
        } else {
            turn = picked;
        }
    }
    out->deadlock = w.deadlock || !w.act[0].done || !w.act[1].done || !w.act[2].done;
    out->max_read = w.max_read;
    return 0;
}

int k_pv_id(const char *name) {
    if (name == NULL) {
        return -1;
    }
    if (strcmp(name, "pc") == 0) {
        return K_PV_PC;
    }
    if (strcmp(name, "pc-wrong") == 0) {
        return K_PV_WRONG;
    }
    if (strcmp(name, "read") == 0) {
        return K_PV_READ;
    }
    if (strcmp(name, "phi") == 0) {
        return K_PV_PHI;
    }
    if (strcmp(name, "phi-room") == 0) {
        return K_PV_ROOM;
    }
    return -1;
}

const char *k_pv_name(int kind) {
    static const char *names[] = {"pc", "pc-wrong", "read", "phi", "phi-room"};
    if (kind < 0 || kind >= K_PV_N) {
        return NULL;
    }
    return names[kind];
}

int k_pv_run(int kind, KPvOut *out) {
    if (out == NULL || kind < 0 || kind >= K_PV_N) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    if (kind == K_PV_PC || kind == K_PV_WRONG) {
        return run_pc(kind == K_PV_WRONG, out);
    }
    if (kind == K_PV_READ) {
        return run_read(out);
    }
    return run_phi(kind == K_PV_ROOM, out);
}

