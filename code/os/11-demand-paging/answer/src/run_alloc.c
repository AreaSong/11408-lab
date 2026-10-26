#include "run.h"

#include "alloc.h"

#include <stdio.h>
#include <string.h>

static Alloc g_alloc;

static void print_pair(int value) {
    if (value < 0) {
        printf("-");
    } else {
        printf("%d", value);
    }
}

static void print_ref(int pid, int page, const AllocRef *ref) {
    printf("ref pid=%d page=%d result=%s cap=%d pool=%d grew=%s victim=", pid, page,
           ref->hit ? "hit" : "fault", ref->cap, ref->pool, ref->grew ? "yes" : "no");
    print_pair(ref->victim_page);
    printf(" from=");
    print_pair(ref->victim_pid);
    printf(" faults=%d replaces=%d\n", ref->faults, ref->replaces);
}

static int cmd_begin(char tok[][TOK_W], int ntok) {
    unsigned long long header[2];
    int base[ALLOC_PROC];
    int policy;
    int nproc;
    int i;

    if (ntok < 5) {
        return fail("bad-command");
    }
    policy = alloc_named(tok[2]);
    if (policy <= 0) {
        return fail("bad-policy");
    }
    if (read_args(tok, 3, 2, header)) {
        return 1;
    }
    if (header[0] < 1 || header[0] > ALLOC_SLOTS || header[1] < 1 || header[1] > ALLOC_PROC) {
        return fail("bad-number");
    }
    nproc = (int)header[1];
    if (ntok != 5 + nproc) {
        return fail("bad-command");
    }
    for (i = 0; i < nproc; i++) {
        unsigned long long value = 0;
        if (read_ull(tok[5 + i], &value) != 0 || value > 1000) {
            return fail("bad-number");
        }
        base[i] = (int)value;
    }
    if (alloc_init(&g_alloc, policy, (int)header[0], nproc, base) != 0) {
        return fail("bad-number");
    }
    printf("alloc policy=%s frames=%d nproc=%d pool=%d\n", alloc_policy_name(policy), g_alloc.frames,
           g_alloc.nproc, g_alloc.pool);
    return 0;
}

static int cmd_ref(char tok[][TOK_W], int ntok) {
    unsigned long long num[2];
    AllocRef ref;

    if (need_args(ntok, 4)) {
        return 1;
    }
    if (!g_alloc.ready) {
        return fail("no-alloc");
    }
    if (read_args(tok, 2, 2, num)) {
        return 1;
    }
    if (num[0] > ALLOC_PROC || num[1] > 1000000) {
        return fail("bad-number");
    }
    if (alloc_ref(&g_alloc, (int)num[0], (int)num[1], &ref) != 0) {
        return fail("bad-number");
    }
    print_ref((int)num[0], (int)num[1], &ref);
    return 0;
}

static int cmd_end(char tok[][TOK_W], int ntok) {
    int i;
    int faults = 0;
    int replaces = 0;

    (void)tok;

    if (need_args(ntok, 2)) {
        return 1;
    }
    if (!g_alloc.ready) {
        return fail("no-alloc");
    }
    printf("summary policy=%s frames=%d pool=%d\n", alloc_policy_name(g_alloc.policy), g_alloc.frames,
           g_alloc.pool);
    for (i = 0; i < g_alloc.nproc; i++) {
        printf("proc pid=%d faults=%d replaces=%d cap=%d\n", i, g_alloc.proc[i].faults,
               g_alloc.proc[i].replaces, g_alloc.proc[i].cap);
        faults += g_alloc.proc[i].faults;
        replaces += g_alloc.proc[i].replaces;
    }
    printf("total-faults=%d total-replaces=%d\n", faults, replaces);
    return 0;
}

int cmd_alloc(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "begin") == 0) {
        return cmd_begin(tok, ntok);
    }
    if (strcmp(tok[1], "ref") == 0) {
        return cmd_ref(tok, ntok);
    }
    if (strcmp(tok[1], "end") == 0) {
        return cmd_end(tok, ntok);
    }
    return fail("bad-command");
}
