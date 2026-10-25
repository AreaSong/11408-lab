#include "model.h"

#include <stdio.h>
#include <string.h>

void scene_clear(Scene *s) {
    if (s != NULL) {
        memset(s, 0, sizeof(*s));
    }
}

void pv_init(PvWorld *w) {
    memset(w, 0, sizeof(*w));
}

int pv_sem(PvWorld *w, const char *name, int value) {
    PvSem *s = &w->sem[w->nsem];
    snprintf(s->name, sizeof s->name, "%s", name);
    s->value = value;
    s->init = value;
    return w->nsem++;
}

int pv_proc(PvWorld *w, const char *name) {
    PvProc *p = &w->proc[w->nproc];
    snprintf(p->name, sizeof p->name, "%s", name);
    p->blocked = -1;
    return w->nproc++;
}

void pv_print_sems(const PvWorld *w) {
    int i;
    for (i = 0; i < w->nsem; i++) {
        printf("sem %s=%d\n", w->sem[i].name, w->sem[i].init);
    }
}

void pv_print(const PvWorld *w) {
    int i;
    for (i = 0; i < w->ntrace; i++) {
        puts(w->trace[i]);
    }
}

static void pv_line(PvWorld *w, int who, const char *op, int sem, int show, int value,
                    const char *state, int wake, const char *note) {
    char vbuf[16];
    const char *sem_name = "-";
    const char *wake_name = "-";
    if (w->ntrace >= PV_TRACE) {
        w->too_long = 1;
        return;
    }
    w->step++;
    if (sem >= 0) {
        sem_name = w->sem[sem].name;
    }
    if (wake >= 0) {
        wake_name = w->proc[wake].name;
    }
    if (show) {
        snprintf(vbuf, sizeof vbuf, "%d", value);
    } else {
        snprintf(vbuf, sizeof vbuf, "-");
    }
    snprintf(w->trace[w->ntrace], PV_LINE,
             "step=%d who=%s op=%s sem=%s value=%s state=%s wake=%s note=%s", w->step,
             w->proc[who].name, op, sem_name, vbuf, state, wake_name,
             note != NULL ? note : "-");
    w->ntrace++;
}

static void drop_hold(PvWorld *w, int who, int sem) {
    int i;
    if (w->hold[who][sem] > 0) {
        w->hold[who][sem]--;
        return;
    }
    /* 第一个读者 P、最后一个读者 V 时，V 的人自己没拿着这把锁。 */
    for (i = 0; i < w->nproc; i++) {
        if (w->hold[i][sem] > 0) {
            w->hold[i][sem]--;
            return;
        }
    }
}

int pv_P(PvWorld *w, int who, int sem) {
    PvSem *s = &w->sem[sem];
    s->value--;
    if (s->value < 0) {
        s->q[s->qn++] = who;
        w->proc[who].blocked = sem;
        pv_line(w, who, "P", sem, 1, s->value, "block", -1, "-");
        return 0;
    }
    w->hold[who][sem]++;
    pv_line(w, who, "P", sem, 1, s->value, "run", -1, "-");
    return 1;
}

int pv_V(PvWorld *w, int who, int sem) {
    PvSem *s = &w->sem[sem];
    int wake = -1;
    const char *state = "inc";
    s->value++;
    drop_hold(w, who, sem);
    if (s->value <= 0 && s->qn > 0) {
        int i;
        wake = s->q[0];
        for (i = 1; i < s->qn; i++) {
            s->q[i - 1] = s->q[i];
        }
        s->qn--;
        w->proc[wake].blocked = -1;
        state = "wake";
    }
    pv_line(w, who, "V", sem, 1, s->value, state, wake, "-");
    return 1;
}

int pv_wait(PvWorld *w, int who, int sem, int *mark) {
    if (!*mark) {
        if (!pv_P(w, who, sem)) {
            *mark = 1;
            return 1;
        }
        *mark = 1;
    }
    /* 阻塞时 P 已经做完。被叫醒后从这里继续，不能再 P 一次。 */
    *mark = 0;
    return 0;
}

void pv_work(PvWorld *w, int who, const char *note) {
    pv_line(w, who, "work", -1, 0, 0, "run", -1, note);
}

void pv_count(PvWorld *w, int who, const char *note, int value) {
    pv_line(w, who, "count", -1, 1, value, "run", -1, note);
}

int pv_prog_add(PvProg *p, int kind, int sem, const char *note) {
    PvIns *ins;
    if (p->n >= PV_PROG) {
        return -1;
    }
    ins = &p->ins[p->n++];
    ins->kind = kind;
    ins->sem = sem;
    snprintf(ins->note, sizeof ins->note, "%s", note != NULL ? note : "-");
    return 0;
}

int pv_exec(PvWorld *w, void *ctx, int who) {
    PvProg *prog = &((PvProg *)ctx)[who];
    PvIns *ins;
    if (prog->pc >= prog->n) {
        return PV_DONE;
    }
    ins = &prog->ins[prog->pc];
    if (ins->kind == PV_OP_P) {
        if (pv_wait(w, who, ins->sem, &prog->mark)) {
            return PV_BLOCK;
        }
        prog->pc++;
        return PV_YIELD;
    }
    if (ins->kind == PV_OP_V) {
        pv_V(w, who, ins->sem);
        prog->pc++;
        return PV_YIELD;
    }
    pv_work(w, who, ins->note);
    prog->pc++;
    return PV_YIELD;
}

static int pv_alive(const PvWorld *w) {
    int i;
    for (i = 0; i < w->nproc; i++) {
        if (!w->proc[i].done) {
            return 1;
        }
    }
    return 0;
}

static int pv_pick(const PvWorld *w) {
    int i;
    for (i = 0; i < w->nproc; i++) {
        if (!w->proc[i].done && w->proc[i].blocked < 0) {
            return i;
        }
    }
    return -1;
}

int pv_sched(PvWorld *w, void *ctx, PvTurn turn) {
    int guard = 0;
    while (guard++ < 800) {
        int who;
        int rc;
        if (!pv_alive(w)) {
            return 1;
        }
        who = pv_pick(w);
        if (who < 0) {
            w->deadlock = 1;
            return 0;
        }
        rc = turn(w, ctx, who);
        if (rc == PV_DONE) {
            w->proc[who].done = 1;
        }
    }
    w->deadlock = 1;
    w->too_long = 1;
    return 0;
}

static int line_has(const char *line, const char *key, const char *value) {
    char needle[64];
    snprintf(needle, sizeof needle, "%s=%s", key, value);
    return strstr(line, needle) != NULL;
}

int pv_count_note(const PvWorld *w, const char *note) {
    int i;
    int n = 0;
    for (i = 0; i < w->ntrace; i++) {
        if (line_has(w->trace[i], "op", "work") && line_has(w->trace[i], "note", note)) {
            n++;
        }
    }
    return n;
}

int pv_works(const PvWorld *w, char out[][PV_NAME], int max) {
    int i;
    int n = 0;
    for (i = 0; i < w->ntrace && n < max; i++) {
        const char *p = strstr(w->trace[i], "note=");
        if (!line_has(w->trace[i], "op", "work") || p == NULL) {
            continue;
        }
        snprintf(out[n], PV_NAME, "%s", p + 5);
        n++;
    }
    return n;
}

static void holders(const PvWorld *w, int sem, char *buf, size_t nbuf) {
    int i;
    size_t used = 0;
    buf[0] = '\0';
    for (i = 0; i < w->nproc; i++) {
        int wrote;
        if (w->hold[i][sem] <= 0) {
            continue;
        }
        wrote = snprintf(buf + used, nbuf - used, "%s%s", used ? "," : "", w->proc[i].name);
        if (wrote < 0 || (size_t)wrote >= nbuf - used) {
            return;
        }
        used += (size_t)wrote;
    }
    if (used == 0) {
        snprintf(buf, nbuf, "-");
    }
}

void pv_print_waits(const PvWorld *w) {
    int i;
    for (i = 0; i < w->nproc; i++) {
        char buf[64];
        int sem = w->proc[i].blocked;
        if (sem < 0) {
            continue;
        }
        holders(w, sem, buf, sizeof buf);
        printf("wait who=%s sem=%s holder=%s\n", w->proc[i].name, w->sem[sem].name, buf);
    }
}
