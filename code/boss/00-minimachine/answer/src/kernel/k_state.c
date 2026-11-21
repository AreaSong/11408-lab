#include "kernel/k_i.h"

#include <string.h>

static const char *ALGO[] = {
    "fcfs", "sjf", "srtn", "hrrn", "rr", "prio-np", "prio-p", "mlfq", "fb2"
};

static const char *CTX[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "pc", "psw", "ptbr"
};

static const char *STATE[] = {"new", "ready", "running", "blocked", "terminated"};

typedef struct {
    const char *from;
    const char *to;
    const char *how;
} Arrow;

/* 与 os/02 同一张图：阻塞不能直接运行，就绪不能直接阻塞。 */
static const Arrow LEGAL[] = {
    {"new", "ready", "admit"},
    {"new", "terminated", "revoke"},
    {"ready", "running", "dispatch"},
    {"ready", "terminated", "revoke"},
    {"running", "ready", "preempt-or-yield"},
    {"running", "blocked", "block"},
    {"running", "terminated", "exit-or-revoke"},
    {"blocked", "ready", "wakeup"},
    {"blocked", "terminated", "revoke"},
};

const char *k_algo_name(int algo) {
    if (algo < 0 || algo >= K_ALGO_N) {
        return "?";
    }
    return ALGO[algo];
}

int k_algo_id(const char *name) {
    int i;
    if (name == NULL) {
        return -1;
    }
    for (i = 0; i < K_ALGO_N; i++) {
        if (strcmp(name, ALGO[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int k_when(int algo, int when) {
    int slice = algo == K_RR || algo == K_MLFQ || algo == K_FB2;
    int arrive = algo == K_SRTN || algo == K_PRIO_P || algo == K_MLFQ || algo == K_FB2;
    if (when == K_WHEN_EXIT || when == K_WHEN_BLOCK) {
        return 1;
    }
    if (when == K_WHEN_SLICE) {
        return slice;
    }
    if (when == K_WHEN_ARRIVE) {
        return arrive;
    }
    return 0;
}

int k_ctx_count(void) {
    return (int)(sizeof CTX / sizeof CTX[0]);
}

const char *k_ctx_field(int index) {
    if (index < 0 || index >= k_ctx_count()) {
        return NULL;
    }
    return CTX[index];
}

static int known_state(const char *name) {
    int i;
    for (i = 0; i < 5; i++) {
        if (strcmp(name, STATE[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int k_trans_ok(const char *from, const char *to, const char **how) {
    int i;
    if (how != NULL) {
        *how = NULL;
    }
    if (from == NULL || to == NULL || !known_state(from) || !known_state(to)) {
        return -1;
    }
    for (i = 0; i < (int)(sizeof LEGAL / sizeof LEGAL[0]); i++) {
        if (strcmp(from, LEGAL[i].from) == 0 && strcmp(to, LEGAL[i].to) == 0) {
            if (how != NULL) {
                *how = LEGAL[i].how;
            }
            return 1;
        }
    }
    if (how != NULL) {
        *how = "-";
    }
    return 0;
}
