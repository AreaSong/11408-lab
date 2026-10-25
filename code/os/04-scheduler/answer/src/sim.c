#include "sched.h"

#include "frac.h"
#include "policy.h"
#include "prep.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    SchedResult *r;
    long long remain[SCHED_MAX];
    long long start_at[SCHED_MAX];
    long long finish_at[SCHED_MAX];
    int admitted[SCHED_MAX];
    int ready[SCHED_MAX];
    int q[SCHED_MAX];
    int qh;
    int qn;
    int running;
    int pending;
    int finished;
    long long slice_used;
    long long time;
    long long limit;
    long long step_max;
} Sim;

static int fail(SchedResult *r, const char *msg) {
    snprintf(r->error, sizeof r->error, "%s", msg);
    return -1;
}

static void sim_bind(Sim *s, SchedResult *r) {
    int i;
    long long latest = 0;
    long long sum = 0;
    memset(s, 0, sizeof *s);
    s->r = r;
    s->running = -1;
    s->pending = -1;
    for (i = 0; i < r->n; i++) {
        long long arrive = r->jobs[i].arrive;
        s->remain[i] = r->jobs[i].service;
        s->start_at[i] = -1;
        s->finish_at[i] = -1;
        sum += r->jobs[i].service;
        if (arrive > latest) {
            latest = arrive;
        }
    }
    s->limit = latest + sum + (long long)r->n * r->switch_cost + 5;
    s->step_max = sum + (long long)r->n * 3 + 10;
}

static int add_seg(Sim *s, int kind, long long from, long long to) {
    SchedSeg *seg;
    if (from == to) {
        return 0;
    }
    if (s->r->nseg > 0) {
        seg = &s->r->segs[s->r->nseg - 1];
        if (seg->kind == kind && seg->end == from) {
            seg->end = to;
            return 0;
        }
    }
    if (s->r->nseg >= SCHED_SEG_MAX) {
        return fail(s->r, "too-long");
    }
    seg = &s->r->segs[s->r->nseg++];
    seg->start = from;
    seg->end = to;
    seg->kind = kind;
    return 0;
}

static int trace_add(Sim *s, const char *line) {
    if (s->r->ntrace >= SCHED_TRACE_MAX || (int)strlen(line) >= SCHED_TRACE_LEN) {
        return fail(s->r, "too-long");
    }
    memcpy(s->r->trace[s->r->ntrace], line, strlen(line) + 1);
    s->r->ntrace++;
    return 0;
}

static int buf_add(char *buf, int cap, int *used, const char *text) {
    int n = (int)strlen(text);
    if (*used + n >= cap) {
        return -1;
    }
    memcpy(buf + *used, text, (size_t)n);
    *used += n;
    buf[*used] = '\0';
    return 0;
}

static int q_push(Sim *s, int id) {
    if (s->qn >= SCHED_MAX) {
        return fail(s->r, "stuck");
    }
    s->q[(s->qh + s->qn) % SCHED_MAX] = id;
    s->qn++;
    return 0;
}

static int q_pop(Sim *s) {
    int id = s->q[s->qh];
    s->qh = (s->qh + 1) % SCHED_MAX;
    s->qn--;
    return id;
}

static int admit_one(Sim *s, int id) {
    s->admitted[id] = 1;
    if (policy_uses_queue(s->r->algo)) {
        return q_push(s, id);
    }
    s->ready[id] = 1;
    return 0;
}

static int earlier(const Sim *s, int a, int b) {
    long long aa = s->r->jobs[a].arrive;
    long long bb = s->r->jobs[b].arrive;
    if (aa != bb) {
        return aa < bb;
    }
    return a < b;
}

static int admit_initial(Sim *s) {
    int ids[SCHED_MAX];
    int n = 0;
    int i, j;
    for (i = 0; i < s->r->n; i++) {
        if (s->r->jobs[i].arrive <= 0) {
            ids[n++] = i;
        }
    }
    for (i = 1; i < n; i++) {
        int value = ids[i];
        for (j = i; j > 0 && earlier(s, value, ids[j - 1]); j--) {
            ids[j] = ids[j - 1];
        }
        ids[j] = value;
    }
    for (i = 0; i < n; i++) {
        if (admit_one(s, ids[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int admit_eq(Sim *s) {
    int i;
    for (i = 0; i < s->r->n; i++) {
        if (!s->admitted[i] && s->r->jobs[i].arrive == s->time) {
            if (admit_one(s, i) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

/* (lo, hi]：切换跳过的时刻也要入队，否则到达正好落在切换结束点的进程会丢。 */
static int admit_between(Sim *s, long long lo, long long hi) {
    for (;;) {
        int i, pick = -1;
        for (i = 0; i < s->r->n; i++) {
            long long arrive = s->r->jobs[i].arrive;
            if (s->admitted[i] || arrive <= lo || arrive > hi) {
                continue;
            }
            if (pick < 0 || earlier(s, i, pick)) {
                pick = i;
            }
        }
        if (pick < 0) {
            return 0;
        }
        if (admit_one(s, pick) != 0) {
            return -1;
        }
    }
}

static int has_ready(const Sim *s) {
    int i;
    if (policy_uses_queue(s->r->algo)) {
        return s->qn > 0;
    }
    for (i = 0; i < s->r->n; i++) {
        if (s->ready[i]) {
            return 1;
        }
    }
    return 0;
}

static int best_ready(const Sim *s) {
    int i, best = -1;
    for (i = 0; i < s->r->n; i++) {
        if (!s->ready[i]) {
            continue;
        }
        if (best < 0 || policy_better(s->r->jobs, s->remain, s->time, s->r->order, s->r->algo, i, best)) {
            best = i;
        }
    }
    return best;
}

static int format_score(const Sim *s, int id, char *buf, size_t n) {
    const SchedJob *job = &s->r->jobs[id];
    if (s->r->algo == SCHED_HRRN) {
        char ratio[64];
        Frac value = frac_make(s->time - job->arrive + job->service, job->service);
        frac_fmt(ratio, sizeof ratio, value);
        return snprintf(buf, n, "%s=%s", job->name, ratio) >= (int)n ? -1 : 0;
    }
    if (s->r->algo == SCHED_SRTN) {
        return snprintf(buf, n, "%s=%lld", job->name, s->remain[id]) >= (int)n ? -1 : 0;
    }
    if (s->r->algo == SCHED_SJF) {
        return snprintf(buf, n, "%s=%lld", job->name, job->service) >= (int)n ? -1 : 0;
    }
    return snprintf(buf, n, "%s=%lld", job->name, job->prio) >= (int)n ? -1 : 0;
}

static int trace_queue(Sim *s) {
    char buf[SCHED_TRACE_LEN];
    char piece[SCHED_NAME + 8];
    int used = 0;
    int i;
    snprintf(piece, sizeof piece, "ready %lld queue", s->time);
    if (buf_add(buf, SCHED_TRACE_LEN, &used, piece) != 0) {
        return fail(s->r, "too-long");
    }
    for (i = 0; i < s->qn; i++) {
        int id = s->q[(s->qh + i) % SCHED_MAX];
        snprintf(piece, sizeof piece, " %s", s->r->jobs[id].name);
        if (buf_add(buf, SCHED_TRACE_LEN, &used, piece) != 0) {
            return fail(s->r, "too-long");
        }
    }
    snprintf(piece, sizeof piece, " -> %s", s->r->jobs[s->q[s->qh]].name);
    if (buf_add(buf, SCHED_TRACE_LEN, &used, piece) != 0) {
        return fail(s->r, "too-long");
    }
    return trace_add(s, buf);
}

static int trace_scored(Sim *s, int chosen) {
    char buf[SCHED_TRACE_LEN];
    char piece[80];
    int used = 0;
    int i;
    snprintf(piece, sizeof piece, "ready %lld %s", s->time, policy_kind(s->r->algo));
    if (buf_add(buf, SCHED_TRACE_LEN, &used, piece) != 0) {
        return fail(s->r, "too-long");
    }
    for (i = 0; i < s->r->n; i++) {
        if (!s->ready[i]) {
            continue;
        }
        if (format_score(s, i, piece, sizeof piece) != 0) {
            return fail(s->r, "too-long");
        }
        if (buf_add(buf, SCHED_TRACE_LEN, &used, " ") != 0 ||
            buf_add(buf, SCHED_TRACE_LEN, &used, piece) != 0) {
            return fail(s->r, "too-long");
        }
    }
    snprintf(piece, sizeof piece, " -> %s", s->r->jobs[chosen].name);
    if (buf_add(buf, SCHED_TRACE_LEN, &used, piece) != 0) {
        return fail(s->r, "too-long");
    }
    return trace_add(s, buf);
}

static int do_dispatch(Sim *s) {
    long long end = s->time + s->r->switch_cost;
    if (add_seg(s, SCHED_DISPATCH, s->time, end) != 0) {
        return -1;
    }
    if (admit_between(s, s->time, end) != 0) {
        return -1;
    }
    s->time = end;
    return 0;
}

static int choose(Sim *s) {
    int sel;
    if (policy_uses_queue(s->r->algo)) {
        if (s->qn <= 0) {
            return fail(s->r, "stuck");
        }
        if (trace_queue(s) != 0) {
            return -1;
        }
        sel = q_pop(s);
    } else {
        sel = best_ready(s);
        if (sel < 0) {
            return fail(s->r, "stuck");
        }
        if (trace_scored(s, sel) != 0) {
            return -1;
        }
        s->ready[sel] = 0;
    }
    if (s->r->switch_cost > 0 && do_dispatch(s) != 0) {
        return -1;
    }
    s->running = sel;
    s->slice_used = 0;
    return 0;
}

static int flush_pending(Sim *s) {
    int id;
    char buf[SCHED_TRACE_LEN];
    if (s->pending < 0) {
        return 0;
    }
    id = s->pending;
    s->pending = -1;
    if (s->qn == 0) {
        snprintf(buf, sizeof buf, "keep %lld %s", s->time, s->r->jobs[id].name);
        if (trace_add(s, buf) != 0) {
            return -1;
        }
        s->running = id;
        s->slice_used = 0;
        return 0;
    }
    return q_push(s, id);
}

static void try_preempt(Sim *s) {
    int i;
    if (s->running < 0) {
        return;
    }
    for (i = 0; i < s->r->n; i++) {
        if (!s->ready[i]) {
            continue;
        }
        if (policy_preempt(s->r->jobs, s->remain, s->r->order, s->r->algo, i, s->running)) {
            s->ready[s->running] = 1;
            s->running = -1;
            return;
        }
    }
}

static int run_one_unit(Sim *s) {
    int id = s->running;
    if (id < 0) {
        return fail(s->r, "stuck");
    }
    if (s->start_at[id] < 0) {
        s->start_at[id] = s->time;
    }
    if (add_seg(s, id, s->time, s->time + 1) != 0) {
        return -1;
    }
    s->remain[id]--;
    s->slice_used++;
    s->time++;
    if (s->remain[id] == 0) {
        s->finish_at[id] = s->time;
        s->running = -1;
        s->finished++;
        return 0;
    }
    if (s->r->algo == SCHED_RR && s->slice_used == s->r->quantum) {
        s->pending = id;
        s->running = -1;
    }
    return 0;
}

static int idle_jump(Sim *s) {
    int i, pick = -1;
    for (i = 0; i < s->r->n; i++) {
        if (s->admitted[i] || s->r->jobs[i].arrive <= s->time) {
            continue;
        }
        if (pick < 0 || earlier(s, i, pick)) {
            pick = i;
        }
    }
    if (pick < 0) {
        return fail(s->r, "stuck");
    }
    if (add_seg(s, SCHED_IDLE, s->time, s->r->jobs[pick].arrive) != 0) {
        return -1;
    }
    s->time = s->r->jobs[pick].arrive;
    return 0;
}

static int sim_loop(Sim *s) {
    long long steps = 0;
    while (s->finished < s->r->n) {
        if (++steps > s->step_max || s->time > s->limit) {
            return fail(s->r, "too-long");
        }
        if (admit_eq(s) != 0 || flush_pending(s) != 0) {
            return -1;
        }
        try_preempt(s);
        if (s->running < 0) {
            if (!has_ready(s)) {
                if (idle_jump(s) != 0) {
                    return -1;
                }
                continue;
            }
            if (choose(s) != 0) {
                return -1;
            }
        }
        if (run_one_unit(s) != 0) {
            return -1;
        }
    }
    return 0;
}

/*
 * 同一时刻的顺序：先让到达的进程入队，再把时间片用完的进程放到队尾。
 * 王道默认新到达的排在前面。剩余时间或优先级只在严格更优时抢占。
 * 非抢占算法的切换开销插在选中之后、第一拍运行之前，这段时间不再改选。
 */
static int fill_stats(Sim *s) {
    int i;
    for (i = 0; i < s->r->n; i++) {
        SchedStat *st = &s->r->stats[i];
        if (s->finish_at[i] < 0 || s->start_at[i] < 0) {
            return fail(s->r, "stuck");
        }
        st->start = s->start_at[i];
        st->finish = s->finish_at[i];
        st->turn = st->finish - s->r->jobs[i].arrive;
        st->wait = st->turn - s->r->jobs[i].service;
        st->response = st->start - s->r->jobs[i].arrive;
    }
    return 0;
}

int sched_run(SchedResult *out, SchedAlgo algo, long long quantum, long long switch_cost,
              PrioOrder order, const SchedJob *jobs, int n) {
    Sim sim;
    if (sched_prepare(out, algo, quantum, switch_cost, order, jobs, n) != 0) {
        return -1;
    }
    sim_bind(&sim, out);
    if (admit_initial(&sim) != 0 || sim_loop(&sim) != 0 || fill_stats(&sim) != 0) {
        return -1;
    }
    return 0;
}
