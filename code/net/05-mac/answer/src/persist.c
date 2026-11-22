#include "persist.h"

#include <string.h>

static int bits_ok(const int *busy, int n) {
    int i;

    if (busy == NULL || n < 1 || n > PERSIST_MAX) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (busy[i] != 0 && busy[i] != 1) {
            return 0;
        }
    }
    return 1;
}

static int push(PersistTrace *out, int slot, int action) {
    if (out->n >= PERSIST_MAX) {
        return 0;
    }
    out->step[out->n].slot = slot;
    out->step[out->n].action = action;
    out->n++;
    return 1;
}

static int run_one(const int *busy, int n, PersistTrace *out) {
    int i;

    for (i = 0; i < n; i++) {
        if (busy[i]) {
            if (!push(out, i, ACT_LISTEN)) {
                return 0;
            }
            continue;
        }
        if (!push(out, i, ACT_SEND)) {
            return 0;
        }
        out->send_slot = i;
        return 1;
    }
    return 1;
}

static int run_non(const int *busy, int n, int wait, PersistTrace *out) {
    int i = 0;
    int k;

    if (wait < 1) {
        return 0;
    }
    while (i < n) {
        if (!busy[i]) {
            if (!push(out, i, ACT_SEND)) {
                return 0;
            }
            out->send_slot = i;
            return 1;
        }
        /* 再听的时隙必须还在序列里，否则这次等待没有结果。 */
        if (i + 1 + wait >= n) {
            return -1;
        }
        if (!push(out, i, ACT_STOP)) {
            return 0;
        }
        for (k = 1; k <= wait; k++) {
            if (!push(out, i + k, ACT_BLIND)) {
                return 0;
            }
        }
        i += 1 + wait;
    }
    return 1;
}

static int run_p(const int *busy, int n, const int *draw, int ndraw, PersistTrace *out) {
    int i;
    int used = 0;

    for (i = 0; i < n; i++) {
        int pick;

        if (busy[i]) {
            if (!push(out, i, ACT_LISTEN)) {
                return 0;
            }
            continue;
        }
        if (draw == NULL || used >= ndraw) {
            return -2;
        }
        pick = draw[used++];
        if (pick != 0 && pick != 1) {
            return 0;
        }
        if (pick == 0) {
            if (!push(out, i, ACT_DEFER)) {
                return 0;
            }
            continue;
        }
        if (!push(out, i, ACT_SEND)) {
            return 0;
        }
        out->send_slot = i;
        return 1;
    }
    return 1;
}

int persist_run(int kind, const int *busy, int nbusy, int wait, const int *draw, int ndraw, PersistTrace *out) {
    if (out == NULL || !bits_ok(busy, nbusy)) {
        return 0;
    }
    memset(out, 0, sizeof *out);
    out->send_slot = -1;
    out->kind = kind;
    out->wait = wait;
    if (kind == PERSIST_ONE) {
        return run_one(busy, nbusy, out);
    }
    if (kind == PERSIST_NON) {
        return run_non(busy, nbusy, wait, out);
    }
    if (kind == PERSIST_P) {
        return run_p(busy, nbusy, draw, ndraw, out);
    }
    return 0;
}
