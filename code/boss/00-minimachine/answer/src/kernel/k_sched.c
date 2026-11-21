#include "kernel/k_i.h"

#include <string.h>

typedef struct {
    KSchedOut *out;
    KJob job[K_JOB_MAX];
    long long remain[K_JOB_MAX];
    long long start[K_JOB_MAX];
    long long finish[K_JOB_MAX];
    int admitted[K_JOB_MAX];
    int n;
    int algo;
    int quantum;
    int large;
    int running;
    int pending;
    int finished;
    long long time;
    long long slice_used;
    CycQ q;
} Sim;

static int fail(Sim *s, const char *msg) {
    snprintf(s->out->error, sizeof s->out->error, "%s", msg);
    return -1;
}

static int better_metric(const Sim *s, int a, int b) {
    long long left;
    long long right;
    const KJob *ja = &s->job[a];
    const KJob *jb = &s->job[b];
    switch (s->algo) {
    case K_SJF:
        if (ja->service != jb->service) {
            return ja->service < jb->service ? 1 : -1;
        }
        return 0;
    case K_SRTN:
        if (s->remain[a] != s->remain[b]) {
            return s->remain[a] < s->remain[b] ? 1 : -1;
        }
        return 0;
    case K_HRRN:
        left = (s->time - ja->arrive + ja->service) * jb->service;
        right = (s->time - jb->arrive + jb->service) * ja->service;
        if (left != right) {
            return left > right ? 1 : -1;
        }
        return 0;
    case K_PRIO_NP:
    case K_PRIO_P:
        if (ja->prio == jb->prio) {
            return 0;
        }
        if (s->large) {
            return ja->prio > jb->prio ? 1 : -1;
        }
        return ja->prio < jb->prio ? 1 : -1;
    default:
        return 0;
    }
}

static int better(const Sim *s, int a, int b) {
    int cmp = better_metric(s, a, b);
    if (cmp != 0) {
        return cmp > 0;
    }
    if (s->job[a].arrive != s->job[b].arrive) {
        return s->job[a].arrive < s->job[b].arrive;
    }
    return a < b;
}

static int beats(const Sim *s, int challenger, int current) {
    if (s->algo == K_SRTN) {
        return s->remain[challenger] < s->remain[current];
    }
    if (s->algo == K_PRIO_P) {
        if (s->job[challenger].prio == s->job[current].prio) {
            return 0;
        }
        if (s->large) {
            return s->job[challenger].prio > s->job[current].prio;
        }
        return s->job[challenger].prio < s->job[current].prio;
    }
    return 0;
}

static int take_best(Sim *s) {
    int ids[K_JOB_MAX];
    int n = 0;
    int x = 0;
    int i;
    int best = 0;
    while (cyc_deq(&s->q, &x)) {
        ids[n++] = x;
    }
    if (n == 0) {
        return -1;
    }
    if (s->algo != K_FCFS && s->algo != K_RR) {
        for (i = 1; i < n; i++) {
            if (better(s, ids[i], ids[best])) {
                best = i;
            }
        }
    }
    for (i = 0; i < n; i++) {
        if (i != best && !cyc_enq(&s->q, ids[i])) {
            return -2;
        }
    }
    return ids[best];
}

static int admit(Sim *s) {
    int ids[K_JOB_MAX];
    int n = 0;
    int i;
    int j;
    for (i = 0; i < s->n; i++) {
        long long arrive = s->job[i].arrive;
        int due = (s->time == 0 && arrive <= 0) || arrive == s->time;
        if (!s->admitted[i] && due) {
            ids[n++] = i;
        }
    }
    for (i = 1; i < n; i++) {
        int value = ids[i];
        for (j = i; j > 0; j--) {
            int prev = ids[j - 1];
            int earlier = s->job[value].arrive < s->job[prev].arrive ||
                          (s->job[value].arrive == s->job[prev].arrive && value < prev);
            if (!earlier) {
                break;
            }
            ids[j] = prev;
        }
        ids[j] = value;
    }
    for (i = 0; i < n; i++) {
        s->admitted[ids[i]] = 1;
        if (!cyc_enq(&s->q, ids[i])) {
            return fail(s, "stuck");
        }
    }
    return 0;
}

static int flush_pending(Sim *s) {
    int id;
    if (s->pending < 0) {
        return 0;
    }
    id = s->pending;
    s->pending = -1;
    if (cyc_empty(&s->q)) {
        s->running = id;
        s->slice_used = 0;
        return 0;
    }
    if (!cyc_enq(&s->q, id)) {
        return fail(s, "stuck");
    }
    return 0;
}

static void try_preempt(Sim *s) {
    int ids[K_JOB_MAX];
    int n = 0;
    int x = 0;
    int i;
    int hit = 0;
    if (s->running < 0 || (s->algo != K_SRTN && s->algo != K_PRIO_P)) {
        return;
    }
    while (cyc_deq(&s->q, &x)) {
        ids[n++] = x;
    }
    for (i = 0; i < n; i++) {
        if (beats(s, ids[i], s->running)) {
            hit = 1;
        }
        cyc_enq(&s->q, ids[i]);
    }
    if (hit) {
        cyc_enq(&s->q, s->running);
        s->running = -1;
    }
}

static int idle_jump(Sim *s) {
    int i;
    int pick = -1;
    for (i = 0; i < s->n; i++) {
        if (s->admitted[i] || s->job[i].arrive <= s->time) {
            continue;
        }
        if (pick < 0 || s->job[i].arrive < s->job[pick].arrive ||
            (s->job[i].arrive == s->job[pick].arrive && i < pick)) {
            pick = i;
        }
    }
    if (pick < 0) {
        return fail(s, "stuck");
    }
    k_seg_add(s->out, s->time, s->job[pick].arrive, "idle");
    s->time = s->job[pick].arrive;
    return s->out->error[0] != '\0' ? -1 : 0;
}

static int run_one(Sim *s) {
    int id = s->running;
    if (id < 0) {
        return fail(s, "stuck");
    }
    if (s->start[id] < 0) {
        s->start[id] = s->time;
    }
    k_seg_add(s->out, s->time, s->time + 1, s->job[id].name);
    if (s->out->error[0] != '\0') {
        return -1;
    }
    s->remain[id]--;
    s->slice_used++;
    s->time++;
    if (s->remain[id] == 0) {
        s->finish[id] = s->time;
        s->running = -1;
        s->finished++;
        return 0;
    }
    if (s->algo == K_RR && s->slice_used == s->quantum) {
        s->pending = id;
        s->running = -1;
    }
    return 0;
}

static int simulate(Sim *s) {
    long long guard = 0;
    long long limit = (long long)s->n * 4 + 8;
    int i;
    for (i = 0; i < s->n; i++) {
        limit += s->job[i].service;
    }
    while (s->finished < s->n) {
        if (++guard > limit) {
            return fail(s, "stuck");
        }
        if (admit(s) != 0 || flush_pending(s) != 0) {
            return -1;
        }
        try_preempt(s);
        if (s->running < 0) {
            if (cyc_empty(&s->q)) {
                if (idle_jump(s) != 0) {
                    return -1;
                }
                continue;
            }
            s->running = take_best(s);
            s->slice_used = 0;
            if (s->running < 0) {
                return fail(s, "stuck");
            }
        }
        if (run_one(s) != 0) {
            return -1;
        }
    }
    return 0;
}

static int prepare(Sim *s, KSchedOut *out, int algo, int quantum, int large, const KJob *jobs, int n) {
    int i;
    int j;
    memset(s, 0, sizeof *s);
    memset(out, 0, sizeof *out);
    s->out = out;
    s->running = -1;
    s->pending = -1;
    s->algo = algo;
    s->quantum = quantum;
    s->large = large;
    s->n = n;
    if (!cyc_init(&s->q, 16)) {
        return fail(s, "stuck");
    }
    if (n < 1) {
        return fail(s, "no-job");
    }
    if (algo == K_RR && (quantum < 1 || quantum > 100000)) {
        return fail(s, "bad-quantum");
    }
    if ((algo == K_PRIO_NP || algo == K_PRIO_P) && large != 0 && large != 1) {
        return fail(s, "bad-order");
    }
    for (i = 0; i < n; i++) {
        if (jobs[i].name[0] == '\0' || strcmp(jobs[i].name, "idle") == 0 || strcmp(jobs[i].name, "end") == 0) {
            return fail(s, "bad-name");
        }
        if (jobs[i].service < 1) {
            return fail(s, "bad-service");
        }
        for (j = 0; j < i; j++) {
            if (strcmp(jobs[j].name, jobs[i].name) == 0) {
                return fail(s, "dup-name");
            }
        }
        s->job[i] = jobs[i];
        s->remain[i] = jobs[i].service;
        s->start[i] = -1;
        s->finish[i] = -1;
    }
    snprintf(out->algo, sizeof out->algo, "%s", k_algo_name(algo));
    return 0;
}

static int fill(Sim *s) {
    long long cpu[K_JOB_MAX];
    int i;
    s->out->n = s->n;
    for (i = 0; i < s->n; i++) {
        if (s->finish[i] < 0) {
            return fail(s, "stuck");
        }
        snprintf(s->out->jname[i], sizeof s->out->jname[i], "%s", s->job[i].name);
        s->out->finish[i] = s->finish[i];
        s->out->turn[i] = s->finish[i] - s->job[i].arrive;
        s->out->wait[i] = s->out->turn[i] - s->job[i].service;
        cpu[i] = s->job[i].service;
    }
    k_avg_fill(s->out, cpu);
    return 0;
}

static int run_basic(KSchedOut *out, int algo, int quantum, int large, const KJob *jobs, int n) {
    Sim sim;
    if (prepare(&sim, out, algo, quantum, large, jobs, n) != 0 || simulate(&sim) != 0 || fill(&sim) != 0) {
        return -1;
    }
    return 0;
}

int k_sched_run(KSchedOut *out, int algo, int quantum, int levels, int large, const KJob *jobs, int n) {
    if (out == NULL || jobs == NULL) {
        return -1;
    }
    if (n > K_JOB_MAX) {
        memset(out, 0, sizeof *out);
        snprintf(out->error, sizeof out->error, "too-many");
        return -1;
    }
    if (algo == K_MLFQ || algo == K_FB2) {
        return k_mlfq_run(out, algo == K_FB2, levels, quantum, jobs, n);
    }
    if (algo < 0 || algo >= K_ALGO_N) {
        memset(out, 0, sizeof *out);
        snprintf(out->error, sizeof out->error, "bad-algo");
        return -1;
    }
    return run_basic(out, algo, quantum, large, jobs, n);
}
