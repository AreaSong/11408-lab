#include "mlfq_int.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    WHY_NONE = 0,
    WHY_ARRIVE,
    WHY_WAKE,
    WHY_EXIT,
    WHY_BLOCK,
    WHY_SLICE,
    WHY_PREEMPT,
    WHY_EMPTY
};

static int fail(MlfqSim *s, const char *msg) {
    snprintf(s->out->error, sizeof s->out->error, "%s", msg);
    return -1;
}

static const char *why_text(int why) {
    switch (why) {
    case WHY_ARRIVE:
        return "arrive";
    case WHY_WAKE:
        return "wake";
    case WHY_EXIT:
        return "exit";
    case WHY_BLOCK:
        return "block";
    case WHY_SLICE:
        return "slice";
    case WHY_PREEMPT:
        return "preempt";
    default:
        return "empty";
    }
}

static int add_trace(MlfqSim *s, const char *fmt, ...) {
    va_list ap;
    int wrote;
    MlfqResult *out = s->out;
    if (out->error[0] != '\0') {
        return -1;
    }
    if (out->ntrace >= MLFQ_TRACE) {
        return fail(s, "too-long");
    }
    va_start(ap, fmt);
    wrote = vsnprintf(out->trace[out->ntrace], MLFQ_LINE, fmt, ap);
    va_end(ap);
    if (wrote < 0 || wrote >= MLFQ_LINE) {
        return fail(s, "too-long");
    }
    out->ntrace++;
    return 0;
}

static int add_seg(MlfqSim *s, const char *name, long long t0, long long t1) {
    MlfqResult *out = s->out;
    if (t1 <= t0) {
        return 0;
    }
    if (out->nseg > 0 && out->gt1[out->nseg - 1] == t0 &&
        strcmp(out->gname[out->nseg - 1], name) == 0) {
        out->gt1[out->nseg - 1] = t1;
        return 0;
    }
    if (out->nseg >= MLFQ_SEG) {
        return fail(s, "too-long");
    }
    snprintf(out->gname[out->nseg], sizeof out->gname[0], "%s", name);
    out->gt0[out->nseg] = t0;
    out->gt1[out->nseg] = t1;
    out->nseg++;
    return 0;
}

static int enqueue(MlfqSim *s, int idx, int level) {
    MlfqProc *job = &s->jobs[idx];
    MlfqQueue *q = &s->q[level];
    if (q->n >= MLFQ_MAX) {
        return fail(s, "stuck");
    }
    job->level = level;
    job->queued = 1;
    job->seq = s->seq++;
    q->id[q->n++] = idx;
    return 0;
}

static int pop_at(MlfqQueue *q, int at) {
    int id = q->id[at];
    int i;
    for (i = at + 1; i < q->n; i++) {
        q->id[i - 1] = q->id[i];
    }
    q->n--;
    return id;
}

/* 短进程优先看剩余 CPU。一样长时，先进入这个队列的先跑。 */
static int take_sjf(MlfqSim *s, int level) {
    MlfqQueue *q = &s->q[level];
    int best = 0;
    int i;
    for (i = 1; i < q->n; i++) {
        const MlfqProc *a = &s->jobs[q->id[i]];
        const MlfqProc *b = &s->jobs[q->id[best]];
        if (a->remain < b->remain || (a->remain == b->remain && a->seq < b->seq)) {
            best = i;
        }
    }
    return pop_at(q, best);
}

static int higher_ready(const MlfqSim *s, int level) {
    int i;
    for (i = 0; i < level; i++) {
        if (s->q[i].n > 0) {
            return 1;
        }
    }
    return 0;
}

static int all_done(const MlfqSim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        if (!s->jobs[i].done) {
            return 0;
        }
    }
    return 1;
}

static long long next_event(const MlfqSim *s, long long now) {
    long long best = -1;
    int i;
    for (i = 0; i < s->n; i++) {
        const MlfqProc *job = &s->jobs[i];
        long long when = -1;
        if (!job->admitted && job->arrive > now) {
            when = job->arrive;
        } else if (job->blocked && job->wake > now) {
            when = job->wake;
        }
        if (when >= 0 && (best < 0 || when < best)) {
            best = when;
        }
    }
    return best;
}

/* 新进程进最高队列队尾。同一时刻按输入顺序，先写的先入队。 */
static int admit(MlfqSim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        MlfqProc *job = &s->jobs[i];
        if (job->admitted || job->arrive != s->now) {
            continue;
        }
        job->admitted = 1;
        s->admitted_now = 1;
        if (enqueue(s, i, 0) != 0) {
            return -1;
        }
        if (add_trace(s, "%lld admit %s q0", s->now, job->name) != 0) {
            return -1;
        }
    }
    return 0;
}

/* 阻塞不算用完时间片，不降级。唤醒后仍回到原来的队列队尾。 */
static int wake_jobs(MlfqSim *s) {
    int i;
    for (i = 0; i < s->n; i++) {
        MlfqProc *job = &s->jobs[i];
        int level;
        if (!job->blocked || job->wake != s->now) {
            continue;
        }
        job->blocked = 0;
        job->phase = 2;
        job->seg = job->cpu2;
        level = job->level;
        s->woke_now = 1;
        if (enqueue(s, i, level) != 0) {
            return -1;
        }
        if (add_trace(s, "%lld wake %s q%d", s->now, job->name, level) != 0) {
            return -1;
        }
    }
    return 0;
}

/*
 * 这一段 CPU 已经结束。后面还有 I/O 就是阻塞，不是降级。
 * 结束和时间片边界撞在一起时走这里：进程并不打算再占着 CPU。
 */
static int finish_or_block(MlfqSim *s, int idx, int *why) {
    MlfqProc *job = &s->jobs[idx];
    s->running = -1;
    if (job->phase == 0 && job->io > 0) {
        job->blocked = 1;
        job->phase = 1;
        job->wake = s->now + job->io;
        if (add_trace(s, "%lld block %s until=%lld", s->now, job->name, job->wake) != 0) {
            return -1;
        }
        *why = WHY_BLOCK;
        return 0;
    }
    job->done = 1;
    job->finish = s->now;
    *why = WHY_EXIT;
    return 0;
}

static int expire(MlfqSim *s, int idx, int *why) {
    MlfqProc *job = &s->jobs[idx];
    int level = job->level;
    int to = level;
    s->running = -1;
    if (level + 1 < s->out->levels) {
        to = level + 1;
        if (add_trace(s, "%lld demote %s q%d->q%d", s->now, job->name, level, to) != 0) {
            return -1;
        }
    } else if (add_trace(s, "%lld requeue %s q%d", s->now, job->name, level) != 0) {
        return -1;
    }
    if (enqueue(s, idx, to) != 0) {
        return -1;
    }
    *why = WHY_SLICE;
    return 0;
}

/* 只有更高队列才抢占。放回本队列队尾，这次时间片作废，下次重新拿满。 */
static int preempt(MlfqSim *s, int idx, int *why) {
    MlfqProc *job = &s->jobs[idx];
    int level = job->level;
    s->running = -1;
    if (add_trace(s, "%lld preempt %s stay q%d", s->now, job->name, level) != 0) {
        return -1;
    }
    if (enqueue(s, idx, level) != 0) {
        return -1;
    }
    *why = WHY_PREEMPT;
    return 0;
}

static int halt(MlfqSim *s, int *why) {
    MlfqProc *job;
    int idx, level;
    *why = WHY_NONE;
    if (s->running < 0) {
        return 0;
    }
    idx = s->running;
    job = &s->jobs[idx];
    level = job->level;
    if (job->seg == 0) {
        return finish_or_block(s, idx, why);
    }
    if (s->out->quantum[level] > 0 && job->used == s->out->quantum[level]) {
        return expire(s, idx, why);
    }
    if (higher_ready(s, level)) {
        return preempt(s, idx, why);
    }
    return 0;
}

static int pick_one(MlfqSim *s, int level) {
    int id = s->out->sjf[level] ? take_sjf(s, level) : pop_at(&s->q[level], 0);
    MlfqProc *job = &s->jobs[id];
    job->queued = 0;
    job->used = 0;
    job->level = level;
    if (job->start < 0) {
        job->start = s->now;
    }
    s->running = id;
    return id;
}

static int pick(MlfqSim *s) {
    int level;
    for (level = 0; level < s->out->levels; level++) {
        if (s->q[level].n == 0) {
            continue;
        }
        return pick_one(s, level);
    }
    s->running = -1;
    return -1;
}

static int trace_schedule(MlfqSim *s, const char *to, int why, int level, int slice, int has_q) {
    const char *sw = strcmp(s->last, to) == 0 ? "no" : "yes";
    int rc;
    if (has_q) {
        rc = add_trace(s, "%lld schedule reason=%s from=%s to=%s switch=%s q=%d slice=%d",
                       s->now, why_text(why), s->last, to, sw, level, slice);
    } else {
        rc = add_trace(s, "%lld schedule reason=%s from=%s to=%s switch=%s", s->now,
                       why_text(why), s->last, to, sw);
    }
    if (rc != 0) {
        return -1;
    }
    snprintf(s->last, sizeof s->last, "%s", to);
    return 0;
}

static int dispatch_trace(MlfqSim *s, int id, int why) {
    MlfqProc *job = &s->jobs[id];
    int slice = s->out->quantum[job->level];
    if (trace_schedule(s, job->name, why, job->level, slice, 1) != 0) {
        return -1;
    }
    s->run_from = s->now;
    snprintf(s->run_name, sizeof s->run_name, "%s", job->name);
    return 0;
}

static int go_idle(MlfqSim *s, int why) {
    long long next;
    if (why == WHY_NONE) {
        why = WHY_EMPTY;
    }
    if (trace_schedule(s, "idle", why, 0, 0, 0) != 0) {
        return -1;
    }
    next = next_event(s, s->now);
    if (next < 0) {
        if (!all_done(s)) {
            return fail(s, "stuck");
        }
        return 0;
    }
    if (add_seg(s, "idle", s->now, next) != 0) {
        return -1;
    }
    s->now = next;
    return 1;
}

static int take_or_idle(MlfqSim *s, int why) {
    int id;
    if (s->running >= 0) {
        return 0;
    }
    id = pick(s);
    if (id >= 0) {
        if (why == WHY_NONE) {
            if (s->admitted_now) {
                why = WHY_ARRIVE;
            } else if (s->woke_now) {
                why = WHY_WAKE;
            } else {
                why = WHY_EMPTY;
            }
        }
        return dispatch_trace(s, id, why);
    }
    return go_idle(s, why);
}

static int burn(MlfqSim *s) {
    MlfqProc *job = &s->jobs[s->running];
    long long step = job->seg;
    int quantum = s->out->quantum[job->level];
    long long next;
    if (quantum > 0) {
        long long left = (long long)quantum - job->used;
        if (left < step) {
            step = left;
        }
    }
    next = next_event(s, s->now);
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

int mlfq_drive(MlfqSim *s) {
    for (;;) {
        int why = WHY_NONE;
        int rc;
        if (s->now > s->limit) {
            return fail(s, "stuck");
        }
        s->admitted_now = 0;
        s->woke_now = 0;
        if (admit(s) != 0 || wake_jobs(s) != 0) {
            return -1;
        }
        if (s->running >= 0) {
            if (halt(s, &why) != 0) {
                return -1;
            }
            if (why != WHY_NONE && add_seg(s, s->run_name, s->run_from, s->now) != 0) {
                return -1;
            }
        }
        rc = take_or_idle(s, why);
        if (rc < 0) {
            return -1;
        }
        if (rc > 0) {
            continue;
        }
        if (s->running < 0) {
            return 0;
        }
        if (burn(s) != 0) {
            return -1;
        }
    }
}

