#include "show.h"

#include "policy.h"

#include <stdio.h>

static const char *order_name(PrioOrder order) {
    if (order == PRIO_LARGE) {
        return "large";
    }
    if (order == PRIO_SMALL) {
        return "small";
    }
    return "none";
}

int show_metrics(const SchedResult *r, Frac *turn, Frac *weight, Frac *wait) {
    int i, bad = 0;
    long long tsum = 0;
    long long wsum = 0;
    Frac acc = frac_make(0, 1);
    for (i = 0; i < r->n; i++) {
        tsum += r->stats[i].turn;
        wsum += r->stats[i].wait;
        if (frac_add(&acc, frac_make(r->stats[i].turn, r->jobs[i].service)) != 0) {
            bad = 1;
        }
    }
    *turn = frac_make(tsum, r->n);
    *wait = frac_make(wsum, r->n);
    if (bad || frac_div_int(acc, r->n, weight) != 0) {
        return -1;
    }
    return 0;
}

static void print_avg(const SchedResult *r, int with_name) {
    Frac turn, weight, wait;
    char tb[64], wb[64], ub[64];
    int bad = show_metrics(r, &turn, &weight, &wait);
    frac_fmt(tb, sizeof tb, turn);
    frac_fmt(ub, sizeof ub, wait);
    if (bad) {
        snprintf(wb, sizeof wb, "overflow");
    } else {
        frac_fmt(wb, sizeof wb, weight);
    }
    if (with_name) {
        printf("%s avg-turn=%s avg-weight=%s avg-wait=%s\n", policy_name(r->algo), tb, wb, ub);
    } else {
        printf("avg turn=%s weight=%s wait=%s\n", tb, wb, ub);
    }
}

static const char *seg_name(const SchedResult *r, int kind) {
    if (kind == SCHED_DISPATCH) {
        return "dispatch";
    }
    if (kind == SCHED_IDLE) {
        return "idle";
    }
    if (kind < 0 || kind >= r->n) {
        return "?";
    }
    return r->jobs[kind].name;
}

void show_run(const SchedResult *r) {
    int i;
    printf("algo=%s q=%lld switch=%lld order=%s\n", policy_name(r->algo), r->quantum, r->switch_cost,
           order_name(r->order));
    printf("trace\n");
    for (i = 0; i < r->ntrace; i++) {
        printf("%s\n", r->trace[i]);
    }
    printf("gantt\n");
    for (i = 0; i < r->nseg; i++) {
        printf("%lld-%lld %s\n", r->segs[i].start, r->segs[i].end, seg_name(r, r->segs[i].kind));
    }
    printf("table\n");
    printf("name arrive service prio start finish turn weight wait response\n");
    for (i = 0; i < r->n; i++) {
        char weight[64];
        const SchedJob *job = &r->jobs[i];
        const SchedStat *st = &r->stats[i];
        frac_fmt(weight, sizeof weight, frac_make(st->turn, job->service));
        printf("%s %lld %lld %lld %lld %lld %lld %s %lld %lld\n", job->name, job->arrive, job->service,
               job->prio, st->start, st->finish, st->turn, weight, st->wait, st->response);
    }
    print_avg(r, 0);
}

void show_pick(const SchedResult *r) {
    int i;
    printf("pick %s at=%lld\n", policy_name(r->algo), r->at);
    printf("ready");
    for (i = 0; i < r->nready; i++) {
        printf(" %s", r->jobs[r->ready_ids[i]].name);
    }
    printf("\nchosen %s\n", r->jobs[r->chosen].name);
}

void show_avg_line(const SchedResult *r) {
    print_avg(r, 1);
}
