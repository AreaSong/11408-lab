#include "mlfq_int.h"

#include "util.h"

#include <stdio.h>
#include <string.h>

static void copy_spec(MlfqResult *out, const MlfqSpec *spec) {
    int i;
    snprintf(out->algo, sizeof out->algo, "%s", spec->algo);
    out->levels = spec->levels;
    out->base_q = spec->base_q;
    for (i = 0; i < MLFQ_LEVELS; i++) {
        out->quantum[i] = spec->quantum[i];
        out->sjf[i] = spec->sjf[i];
    }
}

static void load_job(MlfqProc *job, const MlfqJob *in) {
    memset(job, 0, sizeof *job);
    snprintf(job->name, sizeof job->name, "%s", in->name);
    job->arrive = in->arrive;
    job->cpu1 = in->cpu1;
    job->io = in->io;
    job->cpu2 = in->cpu2;
    job->remain = in->cpu1 + in->cpu2;
    job->seg = in->cpu1;
    job->start = -1;
    job->finish = -1;
}

static void fill_stats(MlfqSim *s) {
    int i;
    s->out->n = s->n;
    for (i = 0; i < s->n; i++) {
        const MlfqProc *job = &s->jobs[i];
        MlfqStat *st = &s->out->stats[i];
        snprintf(s->out->jname[i], sizeof s->out->jname[i], "%s", job->name);
        st->arrive = job->arrive;
        st->service = job->cpu1 + job->cpu2;
        st->start = job->start;
        st->finish = job->finish;
        st->turn = job->finish - job->arrive;
        st->wait = st->turn - st->service - job->io;
        st->response = job->start - job->arrive;
        st->level = job->level;
    }
}

static void print_header(const MlfqResult *out) {
    int i;
    if (strcmp(out->algo, "fb2") == 0) {
        printf("algo=fb2 q0=rr:%d q1=sjf idle=yes\n", out->quantum[0]);
        return;
    }
    printf("algo=mlfq levels=%d q=%d quantum=", out->levels, out->base_q);
    for (i = 0; i < out->levels; i++) {
        printf("%s%d", i == 0 ? "" : ",", out->quantum[i]);
    }
    printf(" idle=yes\n");
}

static void print_table(const MlfqResult *out) {
    long long turn = 0;
    long long wait = 0;
    int i;
    printf("table\n");
    printf("name arrive service start finish turn wait response level\n");
    for (i = 0; i < out->n; i++) {
        const MlfqStat *st = &out->stats[i];
        printf("%s %lld %lld %lld %lld %lld %lld %lld %d\n", out->jname[i], st->arrive,
               st->service, st->start, st->finish, st->turn, st->wait, st->response, st->level);
        turn += st->turn;
        wait += st->wait;
    }
    printf("avg turn=");
    util_print_frac(turn, out->n);
    printf(" wait=");
    util_print_frac(wait, out->n);
    printf("\n");
}

int mlfq_standard(MlfqSpec *spec, int levels, long long q) {
    int i;
    if (levels < 1 || levels > MLFQ_LEVELS) {
        return -1;
    }
    if (q < 1 || q > 100000) {
        return -2;
    }
    memset(spec, 0, sizeof *spec);
    snprintf(spec->algo, sizeof spec->algo, "mlfq");
    spec->levels = levels;
    spec->base_q = (int)q;
    for (i = 0; i < levels; i++) {
        long long slice = q << i;
        if (slice > 1000000) {
            return -2;
        }
        spec->quantum[i] = (int)slice;
    }
    return 0;
}

void mlfq_fb2(MlfqSpec *spec) {
    memset(spec, 0, sizeof *spec);
    snprintf(spec->algo, sizeof spec->algo, "fb2");
    spec->levels = 2;
    spec->base_q = 10;
    spec->quantum[0] = 10;
    spec->quantum[1] = 0;
    spec->sjf[1] = 1;
}

int mlfq_run(const MlfqSpec *spec, const MlfqJob *jobs, int n, MlfqResult *out) {
    MlfqSim sim;
    int i;
    long long latest = 0;
    long long sum = 0;
    if (n < 1 || n > MLFQ_MAX) {
        snprintf(out->error, sizeof out->error, "no-job");
        return -1;
    }
    memset(out, 0, sizeof *out);
    memset(&sim, 0, sizeof sim);
    copy_spec(out, spec);
    sim.out = out;
    sim.n = n;
    sim.running = -1;
    snprintf(sim.last, sizeof sim.last, "-");
    for (i = 0; i < n; i++) {
        load_job(&sim.jobs[i], &jobs[i]);
        sum += jobs[i].cpu1 + jobs[i].io + jobs[i].cpu2;
        if (jobs[i].arrive > latest) {
            latest = jobs[i].arrive;
        }
    }
    sim.limit = latest + sum + 5;
    if (mlfq_drive(&sim) != 0) {
        return -1;
    }
    fill_stats(&sim);
    return 0;
}

void mlfq_print(const MlfqResult *out) {
    int i;
    print_header(out);
    printf("trace\n");
    for (i = 0; i < out->ntrace; i++) {
        printf("%s\n", out->trace[i]);
    }
    printf("gantt\n");
    for (i = 0; i < out->nseg; i++) {
        printf("%lld-%lld %s\n", out->gt0[i], out->gt1[i], out->gname[i]);
    }
    print_table(out);
}
