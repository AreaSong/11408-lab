#include "kernel/k_i.h"

#include <string.h>

typedef struct {
    char name[16];
    long long arrive;
    long long cpu1;
    long long io;
    long long cpu2;
    long long remain;
    long long seg;
    long long used;
    long long wake;
    long long start;
    long long finish;
    int level;
    int phase;
    int admitted;
    int blocked;
    int done;
    int seq;
} MJob;

typedef struct {
    KSchedOut *out;
    MJob job[K_JOB_MAX];
    int n;
    int levels;
    int quantum[4];
    int sjf[4];
    CycQ q[4];
    int running;
    long long now;
    long long run_from;
    long long limit;
    int seq;
    char run_name[16];
} MSim;

static int fail(MSim *s, const char *msg) {
    snprintf(s->out->error, sizeof s->out->error, "%s", msg);
    return -1;
}

static int enqueue(MSim *s, int idx, int level) {
    MJob *job = &s->job[idx];
    job->level = level;
    job->seq = s->seq++;
    if (!cyc_enq(&s->q[level], idx)) {
        return fail(s, "stuck");
    }
    return 0;
}

static int take_front(MSim *s, int level) {
    int id = 0;
    if (!cyc_deq(&s->q[level], &id)) {
        return -1;
    }
    return id;
}

static int take_sjf(MSim *s, int level) {
    int ids[K_JOB_MAX];
    int n = 0;
    int x = 0;
    int best = 0;
    int i;
    while (cyc_deq(&s->q[level], &x)) {
        ids[n++] = x;
    }
    for (i = 1; i < n; i++) {
        MJob *a = &s->job[ids[i]];
        MJob *b = &s->job[ids[best]];
        if (a->remain < b->remain || (a->remain == b->remain && a->seq < b->seq)) {
            best = i;
        }
    }
    for (i = 0; i < n; i++) {
        if (i != best) {
            cyc_enq(&s->q[level], ids[i]);
        }
    }
    return n == 0 ? -1 : ids[best];
}

static int higher(const MSim *s, int level) {
    int i;
    for (i = 0; i < level; i++) {
        if (!cyc_empty(&s->q[i])) {
            return 1;
        }
    }
    return 0;
}

static int admit(MSim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        MJob *job = &s->job[i];
        if (job->admitted || job->arrive != s->now) {
            continue;
        }
        job->admitted = 1;
        if (enqueue(s, i, 0) != 0) {
            return -1;
        }
    }
    return 0;
}

static int wake_jobs(MSim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        MJob *job = &s->job[i];
        if (!job->blocked || job->wake != s->now) {
            continue;
        }
        job->blocked = 0;
        job->phase = 2;
        job->seg = job->cpu2;
        if (enqueue(s, i, job->level) != 0) {
            return -1;
        }
    }
    return 0;
}

static long long next_event(const MSim *s) {
    long long best = -1;
    int i;
    for (i = 0; i < s->n; i++) {
        const MJob *job = &s->job[i];
        long long when = -1;
        if (!job->admitted && job->arrive > s->now) {
            when = job->arrive;
        } else if (job->blocked && job->wake > s->now) {
            when = job->wake;
        }
        if (when >= 0 && (best < 0 || when < best)) {
            best = when;
        }
    }
    return best;
}

static int finish_or_block(MSim *s, int idx) {
    MJob *job = &s->job[idx];
    s->running = -1;
    if (job->phase == 0 && job->io > 0) {
        job->blocked = 1;
        job->phase = 1;
        job->wake = s->now + job->io;
        return 0;
    }
    job->done = 1;
    job->finish = s->now;
    return 0;
}

static int expire(MSim *s, int idx) {
    MJob *job = &s->job[idx];
    int to = job->level;
    s->running = -1;
    if (job->level + 1 < s->levels) {
        to = job->level + 1;
    }
    return enqueue(s, idx, to);
}

static int preempt(MSim *s, int idx) {
    int level = s->job[idx].level;
    s->running = -1;
    return enqueue(s, idx, level);
}

static int halt(MSim *s) {
    MJob *job;
    int level;
    if (s->running < 0) {
        return 0;
    }
    job = &s->job[s->running];
    level = job->level;
    if (job->seg == 0) {
        k_seg_add(s->out, s->run_from, s->now, s->run_name);
        return finish_or_block(s, s->running);
    }
    if (s->quantum[level] > 0 && job->used == s->quantum[level]) {
        k_seg_add(s->out, s->run_from, s->now, s->run_name);
        return expire(s, s->running);
    }
    if (higher(s, level)) {
        k_seg_add(s->out, s->run_from, s->now, s->run_name);
        return preempt(s, s->running);
    }
    return 0;
}

static int pick(MSim *s) {
    int level;
    int id;
    for (level = 0; level < s->levels; level++) {
        if (cyc_empty(&s->q[level])) {
            continue;
        }
        id = s->sjf[level] ? take_sjf(s, level) : take_front(s, level);
        if (id < 0) {
            return -1;
        }
        s->job[id].used = 0;
        s->job[id].level = level;
        if (s->job[id].start < 0) {
            s->job[id].start = s->now;
        }
        s->running = id;
        s->run_from = s->now;
        snprintf(s->run_name, sizeof s->run_name, "%s", s->job[id].name);
        return id;
    }
    s->running = -1;
    return -1;
}

static int go_idle(MSim *s) {
    long long next = next_event(s);
    int i;
    int pending = 0;
    for (i = 0; i < s->n; i++) {
        if (!s->job[i].done) {
            pending = 1;
        }
    }
    if (next < 0) {
        return pending ? fail(s, "stuck") : 0;
    }
    k_seg_add(s->out, s->now, next, "idle");
    s->now = next;
    return 1;
}

static int burn(MSim *s) {
    MJob *job = &s->job[s->running];
    long long step = job->seg;
    int quantum = s->quantum[job->level];
    long long next;
    if (quantum > 0) {
        long long left = (long long)quantum - job->used;
        if (left < step) {
            step = left;
        }
    }
    next = next_event(s);
    if (next > s->now && next - s->now < step) {
        step = next - s->now;
    }
    if (step <= 0) {
        return fail(s, "stuck");
    }
    job->used += step;
    job->seg -= step;
    job->remain -= step;
    s->now += step;
    return 0;
}

static int drive(MSim *s) {
    long long guard = 0;
    while (guard++ <= s->limit) {
        int jumped;
        if (s->now > s->limit) {
            return fail(s, "stuck");
        }
        if (admit(s) != 0 || wake_jobs(s) != 0 || halt(s) != 0) {
            return -1;
        }
        if (s->out->error[0] != '\0') {
            return -1;
        }
        if (s->running < 0 && pick(s) < 0) {
            jumped = go_idle(s);
            if (jumped < 0) {
                return -1;
            }
            if (jumped == 0) {
                return 0;
            }
            continue;
        }
        if (burn(s) != 0) {
            return -1;
        }
    }
    return fail(s, "stuck");
}

static void load_job(MSim *s, int i, const KJob *job) {
    snprintf(s->job[i].name, sizeof s->job[i].name, "%s", job->name);
    s->job[i].arrive = job->arrive;
    s->job[i].cpu1 = job->service;
    s->job[i].io = job->io;
    s->job[i].cpu2 = job->cpu2;
    s->job[i].remain = job->service + job->cpu2;
    s->job[i].seg = job->service;
    s->job[i].start = -1;
    s->job[i].finish = -1;
}

static int setup(MSim *s, KSchedOut *out, int fb2, int levels, int base, const KJob *jobs, int n) {
    int i;
    long long sum = 0;
    memset(s, 0, sizeof *s);
    memset(out, 0, sizeof *out);
    s->out = out;
    s->running = -1;
    s->n = n;
    if (n < 1 || n > K_JOB_MAX) {
        return fail(s, n < 1 ? "no-job" : "too-many");
    }
    if (fb2) {
        s->levels = 2;
        s->quantum[0] = 10;
        s->quantum[1] = 0;
        s->sjf[1] = 1;
        snprintf(out->algo, sizeof out->algo, "fb2");
    } else {
        if (levels < 1 || levels > 4 || base < 1 || base > 10000) {
            return fail(s, "bad-quantum");
        }
        s->levels = levels;
        for (i = 0; i < levels; i++) {
            s->quantum[i] = base << i;
        }
        snprintf(out->algo, sizeof out->algo, "mlfq");
    }
    for (i = 0; i < s->levels; i++) {
        if (!cyc_init(&s->q[i], 16)) {
            return fail(s, "stuck");
        }
    }
    for (i = 0; i < n; i++) {
        if (jobs[i].name[0] == '\0' || jobs[i].service < 1) {
            return fail(s, "bad-job");
        }
        load_job(s, i, &jobs[i]);
        sum += jobs[i].service + jobs[i].io + jobs[i].cpu2;
        if (jobs[i].arrive > s->limit) {
            s->limit = jobs[i].arrive;
        }
    }
    s->limit += sum + 8;
    return 0;
}

static int fill(MSim *s) {
    long long cpu[K_JOB_MAX];
    int i;
    s->out->n = s->n;
    for (i = 0; i < s->n; i++) {
        MJob *job = &s->job[i];
        long long cpu_sum;
        if (job->finish < 0) {
            return fail(s, "stuck");
        }
        cpu_sum = job->cpu1 + job->cpu2;
        snprintf(s->out->jname[i], sizeof s->out->jname[i], "%s", job->name);
        s->out->finish[i] = job->finish;
        s->out->turn[i] = job->finish - job->arrive;
        s->out->wait[i] = s->out->turn[i] - cpu_sum - job->io;
        cpu[i] = cpu_sum;
    }
    k_avg_fill(s->out, cpu);
    return 0;
}

int k_mlfq_run(KSchedOut *out, int fb2, int levels, int base, const KJob *jobs, int n) {
    MSim sim;
    if (out == NULL || jobs == NULL) {
        return -1;
    }
    if (setup(&sim, out, fb2, levels, base, jobs, n) != 0 || drive(&sim) != 0 || fill(&sim) != 0) {
        return -1;
    }
    return 0;
}
