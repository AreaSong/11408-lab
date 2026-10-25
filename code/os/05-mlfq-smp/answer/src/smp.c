#include "smp.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int id[SMP_MAX];
    int n;
} Queue;

typedef struct {
    char name[16];
    long long arrive, service, remain, start, finish;
    int affinity, last, moves, admitted, done;
    char cpus[32];
} Job;

typedef struct {
    SmpResult *out;
    Job jobs[SMP_MAX];
    Queue rq[SMP_CPU];
    int n, shared, balance, aff;
    int run[SMP_CPU];
    long long used[SMP_CPU];
    long long now, slice, limit;
} Sim;

static int fail(Sim *s, const char *msg) {
    snprintf(s->out->error, sizeof s->out->error, "%s", msg);
    return -1;
}

static int add_seg(Sim *s, int cpu, const char *name, long long t0, long long t1) {
    SmpResult *out = s->out;
    int n;
    if (t1 <= t0) {
        return 0;
    }
    n = out->nseg[cpu];
    if (n > 0 && out->t1[cpu][n - 1] == t0 && strcmp(out->gname[cpu][n - 1], name) == 0) {
        out->t1[cpu][n - 1] = t1;
        return 0;
    }
    if (n >= SMP_SEG) {
        return fail(s, "too-long");
    }
    snprintf(out->gname[cpu][n], sizeof out->gname[cpu][n], "%s", name);
    out->t0[cpu][n] = t0;
    out->t1[cpu][n] = t1;
    out->nseg[cpu] = n + 1;
    return 0;
}

static int enqueue(Sim *s, int idx, int slot) {
    Queue *q = &s->rq[slot];
    if (q->n >= SMP_MAX) {
        return fail(s, "stuck");
    }
    q->id[q->n++] = idx;
    return 0;
}

static int remove_at(Queue *q, int at) {
    int id = q->id[at];
    int i;
    for (i = at + 1; i < q->n; i++) {
        q->id[i - 1] = q->id[i];
    }
    q->n--;
    return id;
}

static int allows(const Sim *s, const Job *job, int cpu) {
    if (s->aff == SMP_HARD && job->affinity >= 0 && job->affinity != cpu) {
        return 0;
    }
    return 1;
}

/* 软/硬亲和：优先拿指定 CPU，或上次就在这颗 CPU 上跑过的进程。 */
static int prefers(const Sim *s, const Job *job, int cpu) {
    if (s->aff == SMP_NONE) {
        return 0;
    }
    if (job->affinity == cpu) {
        return 1;
    }
    return job->affinity < 0 && job->last == cpu;
}

static int take_shared(Sim *s, int cpu) {
    Queue *q = &s->rq[0];
    int i, pref = -1, fallback = -1;
    for (i = 0; i < q->n; i++) {
        Job *job = &s->jobs[q->id[i]];
        if (!allows(s, job, cpu)) {
            continue;
        }
        if (fallback < 0) {
            fallback = i;
        }
        if (pref < 0 && prefers(s, job, cpu)) {
            pref = i;
        }
    }
    if (pref >= 0) {
        fallback = pref;
    }
    if (fallback < 0) {
        return -1;
    }
    return remove_at(q, fallback);
}

static int take_private(Sim *s, int cpu) {
    Queue *q = &s->rq[cpu];
    int i;
    for (i = 0; i < q->n; i++) {
        if (allows(s, &s->jobs[q->id[i]], cpu)) {
            return remove_at(q, i);
        }
    }
    return -1;
}

static int place(const Sim *s, int idx) {
    int load0, load1;
    const Job *job = &s->jobs[idx];
    if (s->shared) {
        return 0;
    }
    if (s->aff != SMP_NONE && job->affinity >= 0) {
        return job->affinity;
    }
    load0 = s->rq[0].n + (s->run[0] >= 0);
    load1 = s->rq[1].n + (s->run[1] >= 0);
    return load0 <= load1 ? 0 : 1;
}

static int note_cpu(Sim *s, int idx, int cpu) {
    Job *job = &s->jobs[idx];
    char extra[8];
    int wrote;
    if (job->last >= 0 && job->last != cpu) {
        job->moves++;
    } else if (job->last < 0 && job->affinity >= 0 && s->aff != SMP_NONE && job->affinity != cpu) {
        job->moves++;
    }
    job->last = cpu;
    wrote = snprintf(extra, sizeof extra, "%s%d", job->cpus[0] ? "," : "", cpu);
    if (wrote < 0 || (int)strlen(job->cpus) + wrote >= (int)sizeof job->cpus) {
        return fail(s, "too-long");
    }
    memcpy(job->cpus + strlen(job->cpus), extra, (size_t)wrote + 1);
    return 0;
}

static int admit(Sim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        int slot;
        if (s->jobs[i].admitted || s->jobs[i].arrive != s->now) {
            continue;
        }
        s->jobs[i].admitted = 1;
        slot = place(s, i);
        if (enqueue(s, i, slot) != 0) {
            return -1;
        }
    }
    return 0;
}

static void release_cpu(Sim *s, int cpu) {
    int idx = s->run[cpu];
    Job *job;
    int slot;
    if (idx < 0) {
        return;
    }
    job = &s->jobs[idx];
    if (job->remain == 0) {
        job->done = 1;
        job->finish = s->now;
        s->run[cpu] = -1;
        return;
    }
    if (s->slice > 0 && s->used[cpu] == s->slice) {
        slot = s->shared ? 0 : cpu;
        s->run[cpu] = -1;
        enqueue(s, idx, slot);
    }
}

/*
 * 对方正在跑，或就绪队列里至少还有两个，才从队尾拉走一个。
 * 不抢占正在运行的进程，也不把对方唯一的就绪进程抢走。
 * 软亲和和硬亲和都不把「指定了另一颗 CPU」的进程拉走。
 */
static int can_steal(const Sim *s, const Job *job, int dest) {
    if (s->aff == SMP_NONE) {
        return 1;
    }
    return job->affinity < 0 || job->affinity == dest;
}

static int source_busy(const Sim *s, int src) {
    if (s->rq[src].n <= 0) {
        return 0;
    }
    return s->run[src] >= 0 || s->rq[src].n >= 2;
}

static int balance(Sim *s) {
    int dest;
    if (s->shared || !s->balance) {
        return 0;
    }
    for (dest = 0; dest < SMP_CPU; dest++) {
        int src = 1 - dest;
        int i;
        if (s->run[dest] >= 0 || s->rq[dest].n > 0 || !source_busy(s, src)) {
            continue;
        }
        for (i = s->rq[src].n - 1; i >= 0; i--) {
            if (!can_steal(s, &s->jobs[s->rq[src].id[i]], dest)) {
                continue;
            }
            if (enqueue(s, remove_at(&s->rq[src], i), dest) != 0) {
                return -1;
            }
            break;
        }
    }
    return 0;
}

static int dispatch(Sim *s, int cpu) {
    int id;
    if (s->run[cpu] >= 0) {
        return 0;
    }
    id = s->shared ? take_shared(s, cpu) : take_private(s, cpu);
    if (id < 0) {
        return 0;
    }
    s->run[cpu] = id;
    s->used[cpu] = 0;
    if (s->jobs[id].start < 0) {
        s->jobs[id].start = s->now;
    }
    return note_cpu(s, id, cpu);
}

static int running_any(const Sim *s) {
    return s->run[0] >= 0 || s->run[1] >= 0;
}

static int queues_empty(const Sim *s) {
    if (s->shared) {
        return s->rq[0].n == 0;
    }
    return s->rq[0].n == 0 && s->rq[1].n == 0;
}

static int all_done(const Sim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        if (!s->jobs[i].done) {
            return 0;
        }
    }
    return 1;
}

static long long next_arrive(const Sim *s, long long now) {
    long long best = -1;
    int i;
    for (i = 0; i < s->n; i++) {
        if (!s->jobs[i].admitted && s->jobs[i].arrive > now &&
            (best < 0 || s->jobs[i].arrive < best)) {
            best = s->jobs[i].arrive;
        }
    }
    return best;
}

static int burn(Sim *s) {
    long long step = -1;
    long long next;
    int cpu;
    for (cpu = 0; cpu < SMP_CPU; cpu++) {
        long long left;
        if (s->run[cpu] < 0) {
            continue;
        }
        left = s->jobs[s->run[cpu]].remain;
        if (s->slice > 0 && s->slice - s->used[cpu] < left) {
            left = s->slice - s->used[cpu];
        }
        if (step < 0 || left < step) {
            step = left;
        }
    }
    next = next_arrive(s, s->now);
    if (next > s->now && (step < 0 || next - s->now < step)) {
        step = next - s->now;
    }
    if (step <= 0) {
        return fail(s, "stuck");
    }
    for (cpu = 0; cpu < SMP_CPU; cpu++) {
        const char *name = s->run[cpu] < 0 ? "idle" : s->jobs[s->run[cpu]].name;
        if (add_seg(s, cpu, name, s->now, s->now + step) != 0) {
            return -1;
        }
        if (s->run[cpu] >= 0) {
            s->jobs[s->run[cpu]].remain -= step;
            s->used[cpu] += step;
        }
    }
    s->now += step;
    return 0;
}

static int idle_jump(Sim *s) {
    long long next = next_arrive(s, s->now);
    int cpu;
    if (next < 0) {
        return fail(s, "stuck");
    }
    for (cpu = 0; cpu < SMP_CPU; cpu++) {
        if (add_seg(s, cpu, "idle", s->now, next) != 0) {
            return -1;
        }
    }
    s->now = next;
    return 1;
}

static int drive(Sim *s) {
    for (;;) {
        int cpu;
        if (s->now > s->limit) {
            return fail(s, "stuck");
        }
        if (admit(s) != 0) {
            return -1;
        }
        for (cpu = 0; cpu < SMP_CPU; cpu++) {
            release_cpu(s, cpu);
        }
        if (all_done(s)) {
            return 0;
        }
        if (balance(s) != 0) {
            return -1;
        }
        for (cpu = 0; cpu < SMP_CPU; cpu++) {
            if (dispatch(s, cpu) != 0) {
                return -1;
            }
        }
        if (!running_any(s)) {
            if (!queues_empty(s)) {
                return fail(s, "stuck");
            }
            if (idle_jump(s) < 0) {
                return -1;
            }
            continue;
        }
        if (burn(s) != 0) {
            return -1;
        }
    }
}

static void load_job(Job *job, const SmpJob *in) {
    memset(job, 0, sizeof *job);
    snprintf(job->name, sizeof job->name, "%s", in->name);
    job->arrive = in->arrive;
    job->service = in->service;
    job->remain = in->service;
    job->affinity = in->affinity;
    job->last = -1;
    job->start = -1;
    job->finish = -1;
}

static void fill_stats(Sim *s) {
    int i;
    s->out->n = s->n;
    s->out->migrations = 0;
    s->out->makespan = 0;
    for (i = 0; i < s->n; i++) {
        const Job *job = &s->jobs[i];
        SmpStat *st = &s->out->stats[i];
        snprintf(s->out->jname[i], sizeof s->out->jname[i], "%s", job->name);
        st->arrive = job->arrive;
        st->service = job->service;
        st->affinity = job->affinity;
        st->start = job->start;
        st->finish = job->finish;
        st->moves = job->moves;
        snprintf(st->cpus, sizeof st->cpus, "%s", job->cpus);
        s->out->migrations += job->moves;
        if (job->finish > s->out->makespan) {
            s->out->makespan = job->finish;
        }
    }
}

static const char *aff_name(int aff) {
    if (aff == SMP_SOFT) {
        return "soft";
    }
    if (aff == SMP_HARD) {
        return "hard";
    }
    return "none";
}

static void stamp(SmpResult *out, int shared, int balance, int aff, long long slice) {
    snprintf(out->mode, sizeof out->mode, "%s", shared ? "shared" : "private");
    if (shared) {
        snprintf(out->balance, sizeof out->balance, "shared");
    } else {
        snprintf(out->balance, sizeof out->balance, "%s", balance ? "pull" : "off");
    }
    snprintf(out->affinity, sizeof out->affinity, "%s", aff_name(aff));
    out->slice = slice;
}

int smp_run(int shared, int balance, int aff, long long slice, const SmpJob *jobs, int n,
            SmpResult *out) {
    Sim sim;
    int i;
    long long latest = 0;
    long long sum = 0;
    if (n < 1 || n > SMP_MAX) {
        snprintf(out->error, sizeof out->error, "no-job");
        return -1;
    }
    memset(out, 0, sizeof *out);
    memset(&sim, 0, sizeof sim);
    stamp(out, shared, balance, aff, slice);
    sim.out = out;
    sim.n = n;
    sim.shared = shared;
    sim.balance = balance;
    sim.aff = aff;
    sim.slice = slice;
    sim.run[0] = sim.run[1] = -1;
    for (i = 0; i < n; i++) {
        load_job(&sim.jobs[i], &jobs[i]);
        sum += jobs[i].service;
        if (jobs[i].arrive > latest) {
            latest = jobs[i].arrive;
        }
    }
    sim.limit = latest + sum + 5;
    if (drive(&sim) != 0) {
        return -1;
    }
    fill_stats(&sim);
    return 0;
}
