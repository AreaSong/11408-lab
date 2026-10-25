#include "logic.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int id_ok(const char *s) {
    int n;
    if (s == NULL || !isalpha((unsigned char)s[0])) {
        return 0;
    }
    for (n = 0; s[n] != '\0'; n++) {
        unsigned char c = (unsigned char)s[n];
        if (n >= LOGIC_NAME - 1 || (!isalnum(c) && c != '_')) {
            return 0;
        }
    }
    return strcmp(s, "end") != 0;
}

static int in_queue(const char q[][LOGIC_NAME], int nq, const char *name) {
    int i;
    for (i = 0; i < nq; i++) {
        if (strcmp(q[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void join_q(const char q[][LOGIC_NAME], int nq, char *buf, size_t n) {
    int i;
    size_t used = 0;
    if (nq <= 0) {
        snprintf(buf, n, "-");
        return;
    }
    buf[0] = '\0';
    for (i = 0; i < nq; i++) {
        int wrote = snprintf(buf + used, n - used, "%s%s", i ? "," : "", q[i]);
        if (wrote < 0 || (size_t)wrote >= n - used) {
            return;
        }
        used += (size_t)wrote;
    }
}

static void pop_front(char q[][LOGIC_NAME], int *nq, char *who, size_t n) {
    int i;
    snprintf(who, n, "%s", q[0]);
    for (i = 1; i < *nq; i++) {
        snprintf(q[i - 1], LOGIC_NAME, "%s", q[i]);
    }
    (*nq)--;
}

static void fill_pv(const LogicSem *s, PvView *out, const char *state, const char *who) {
    out->value = s->value;
    out->available = logic_available(s->value);
    out->waiters = logic_waiters(s->value);
    join_q(s->q, s->nq, out->queue, sizeof out->queue);
    snprintf(out->state, sizeof out->state, "%s", state);
    snprintf(out->who, sizeof out->who, "%s", who);
}

int logic_available(int value) {
    return value > 0 ? value : 0;
}

int logic_waiters(int value) {
    return value < 0 ? -value : 0;
}

void logic_sem_init(LogicSem *s, int value) {
    memset(s, 0, sizeof *s);
    s->value = value;
}

void logic_box_init(LogicBox *b) {
    memset(b, 0, sizeof *b);
}

int logic_p(LogicSem *s, const char *name, PvView *out) {
    if (!id_ok(name)) {
        return -1;
    }
    if (in_queue(s->q, s->nq, name)) {
        return -2;
    }
    s->value--;
    if (s->value < 0) {
        if (s->nq >= LOGIC_MAX) {
            s->value++;
            return -3;
        }
        snprintf(s->q[s->nq], LOGIC_NAME, "%s", name);
        s->nq++;
        fill_pv(s, out, "block", name);
        return 0;
    }
    fill_pv(s, out, "run", "-");
    return 0;
}

int logic_v(LogicSem *s, const char *name, PvView *out) {
    char woken[LOGIC_NAME];
    if (!id_ok(name)) {
        return -1;
    }
    s->value++;
    /* 先加 1，再看加完后是不是 <= 0。等于 0 说明刚好把最后一个等待者叫醒。 */
    if (s->value <= 0) {
        if (s->nq <= 0) {
            s->value--;
            return -3;
        }
        pop_front(s->q, &s->nq, woken, sizeof woken);
        fill_pv(s, out, "wake", woken);
        return 0;
    }
    fill_pv(s, out, "inc", "-");
    return 0;
}

static int find_blocked(const LogicBox *b, const char *name) {
    int c;
    for (c = 0; c < b->n; c++) {
        if (in_queue(b->cond[c].q, b->cond[c].nq, name)) {
            return 1;
        }
    }
    return 0;
}

static LogicCond *touch_cond(LogicBox *b, const char *name, int create) {
    int i;
    for (i = 0; i < b->n; i++) {
        if (strcmp(b->cond[i].name, name) == 0) {
            return &b->cond[i];
        }
    }
    if (!create || b->n >= LOGIC_COND) {
        return NULL;
    }
    memset(&b->cond[b->n], 0, sizeof b->cond[b->n]);
    snprintf(b->cond[b->n].name, LOGIC_NAME, "%s", name);
    return &b->cond[b->n++];
}

int logic_wait(LogicBox *b, const char *who, const char *cond, CondView *out) {
    LogicCond *c;
    if (!id_ok(who) || !id_ok(cond)) {
        return -1;
    }
    if (find_blocked(b, who)) {
        return -2;
    }
    c = touch_cond(b, cond, 1);
    if (c == NULL || c->nq >= LOGIC_MAX) {
        return -3;
    }
    snprintf(c->q[c->nq], LOGIC_NAME, "%s", who);
    c->nq++;
    join_q(c->q, c->nq, out->queue, sizeof out->queue);
    snprintf(out->wake, sizeof out->wake, "-");
    snprintf(out->state, sizeof out->state, "block");
    return 0;
}

int logic_signal(LogicBox *b, const char *who, const char *cond, CondView *out) {
    LogicCond *c;
    if (!id_ok(who) || !id_ok(cond)) {
        return -1;
    }
    if (find_blocked(b, who)) {
        return -2;
    }
    c = touch_cond(b, cond, 0);
    if (c == NULL || c->nq == 0) {
        /* 没有等待者时信号丢掉，不像 V 会把计数留到以后。 */
        snprintf(out->queue, sizeof out->queue, "-");
        snprintf(out->wake, sizeof out->wake, "-");
        snprintf(out->state, sizeof out->state, "lost");
        return 0;
    }
    pop_front(c->q, &c->nq, out->wake, sizeof out->wake);
    join_q(c->q, c->nq, out->queue, sizeof out->queue);
    snprintf(out->state, sizeof out->state, "wake");
    return 0;
}
