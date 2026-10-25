#include "prep.h"

#include "policy.h"

#include <stdio.h>
#include <string.h>

static int fail(SchedResult *out, const char *msg) {
    snprintf(out->error, sizeof out->error, "%s", msg);
    return -1;
}

int sched_prepare(SchedResult *out, SchedAlgo algo, long long quantum, long long switch_cost,
                  PrioOrder order, const SchedJob *jobs, int n) {
    int i, j;
    memset(out, 0, sizeof *out);
    out->chosen = -1;
    out->algo = algo;
    out->quantum = quantum;
    out->switch_cost = switch_cost;
    out->order = order;
    if (n < 1) {
        return fail(out, "no-job");
    }
    if (n > SCHED_MAX) {
        return fail(out, "too-many");
    }
    if (algo == SCHED_RR && (quantum < 1 || quantum > 100000)) {
        return fail(out, "bad-quantum");
    }
    if (switch_cost < 0 || switch_cost > 100000) {
        return fail(out, "bad-switch");
    }
    if (switch_cost > 0 && (algo == SCHED_SRTN || algo == SCHED_RR || algo == SCHED_PRIO_P)) {
        return fail(out, "bad-switch");
    }
    if ((algo == SCHED_PRIO_NP || algo == SCHED_PRIO_P) && order == PRIO_NONE) {
        return fail(out, "bad-order");
    }
    out->n = n;
    for (i = 0; i < n; i++) {
        if (!sched_name_ok(jobs[i].name)) {
            return fail(out, "bad-job");
        }
        if (jobs[i].service < 1 || jobs[i].service > 100000) {
            return fail(out, "bad-service");
        }
        if (jobs[i].arrive < -1000000 || jobs[i].arrive > 1000000) {
            return fail(out, "bad-arrive");
        }
        if (jobs[i].prio < -1000000 || jobs[i].prio > 1000000) {
            return fail(out, "bad-prio");
        }
        for (j = 0; j < i; j++) {
            if (strcmp(jobs[j].name, jobs[i].name) == 0) {
                return fail(out, "dup-name");
            }
        }
        out->jobs[i] = jobs[i];
    }
    return 0;
}

int sched_pick(SchedResult *out, SchedAlgo algo, long long at, PrioOrder order, const SchedJob *jobs,
               int n) {
    long long remain[SCHED_MAX];
    int i, best = -1;
    if (algo != SCHED_FCFS && algo != SCHED_SJF && algo != SCHED_HRRN && algo != SCHED_PRIO_NP) {
        memset(out, 0, sizeof *out);
        out->chosen = -1;
        return fail(out, "bad-pick");
    }
    if (sched_prepare(out, algo, 0, 0, order, jobs, n) != 0) {
        return -1;
    }
    out->at = at;
    for (i = 0; i < n; i++) {
        remain[i] = jobs[i].service;
    }
    for (i = 0; i < n; i++) {
        if (out->jobs[i].arrive > at) {
            continue;
        }
        out->ready_ids[out->nready++] = i;
        if (best < 0 || policy_better(out->jobs, remain, at, order, algo, i, best)) {
            best = i;
        }
    }
    if (best < 0) {
        return fail(out, "no-ready");
    }
    out->chosen = best;
    return 0;
}
