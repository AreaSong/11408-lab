#include "exam.h"
#include "frac.h"
#include "policy.h"
#include "sched.h"
#include "show.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static SchedJob job(const char *name, long long arrive, long long service, long long prio) {
    SchedJob item;
    memset(&item, 0, sizeof item);
    snprintf(item.name, sizeof item.name, "%s", name);
    item.arrive = arrive;
    item.service = service;
    item.prio = prio;
    return item;
}

static int trace_has(const SchedResult *r, const char *line) {
    int i;
    for (i = 0; i < r->ntrace; i++) {
        if (strcmp(r->trace[i], line) == 0) {
            return 1;
        }
    }
    return 0;
}

static int seg_is(const SchedResult *r, int index, long long start, long long end, int kind) {
    const SchedSeg *seg;
    if (index < 0 || index >= r->nseg) {
        return 0;
    }
    seg = &r->segs[index];
    return seg->start == start && seg->end == end && seg->kind == kind;
}

static int who_at(const SchedResult *r, long long t) {
    int i;
    for (i = 0; i < r->nseg; i++) {
        if (r->segs[i].start <= t && t < r->segs[i].end) {
            return r->segs[i].kind;
        }
    }
    return -3;
}

static long long turn_sum(const SchedResult *r) {
    long long sum = 0;
    int i;
    for (i = 0; i < r->n; i++) {
        sum += r->stats[i].turn;
    }
    return sum;
}

static void expect_avg_weight(const SchedResult *r, long long num, long long den) {
    Frac turn, weight, wait;
    Frac want = frac_make(num, den);
    CHECK(show_metrics(r, &turn, &weight, &wait) == 0);
    CHECK_EQ(weight.n, want.n);
    CHECK_EQ(weight.d, want.d);
}

static void textbook(SchedJob out[4]) {
    out[0] = job("P1", 0, 7, 0);
    out[1] = job("P2", 2, 4, 0);
    out[2] = job("P3", 4, 1, 0);
    out[3] = job("P4", 5, 4, 0);
}

static void test_fcfs_sjf_srtn_hrrn(void) {
    SchedJob jobs[4];
    SchedResult r;
    textbook(jobs);
    CHECK(sched_run(&r, SCHED_FCFS, 0, 0, PRIO_NONE, jobs, 4) == 0);
    CHECK(seg_is(&r, 0, 0, 7, 0));
    CHECK(seg_is(&r, 1, 7, 11, 1));
    CHECK(seg_is(&r, 2, 11, 12, 2));
    CHECK(seg_is(&r, 3, 12, 16, 3));
    CHECK_EQ(turn_sum(&r), 35);
    CHECK_EQ(r.stats[1].turn, 9);
    CHECK_EQ(r.stats[1].wait, 5);
    expect_avg_weight(&r, 7, 2);
    CHECK(trace_has(&r, "ready 7 queue P2 P3 P4 -> P2"));

    CHECK(sched_run(&r, SCHED_SJF, 0, 0, PRIO_NONE, jobs, 4) == 0);
    CHECK(seg_is(&r, 0, 0, 7, 0));
    CHECK(seg_is(&r, 1, 7, 8, 2));
    CHECK(seg_is(&r, 2, 8, 12, 1));
    CHECK(seg_is(&r, 3, 12, 16, 3));
    CHECK_EQ(turn_sum(&r), 32);
    CHECK_EQ(r.stats[2].turn, 4);
    expect_avg_weight(&r, 41, 16);
    CHECK(trace_has(&r, "ready 8 service P2=4 P4=4 -> P2"));

    CHECK(sched_run(&r, SCHED_SRTN, 0, 0, PRIO_NONE, jobs, 4) == 0);
    CHECK(seg_is(&r, 0, 0, 2, 0));
    CHECK(seg_is(&r, 1, 2, 4, 1));
    CHECK(seg_is(&r, 2, 4, 5, 2));
    CHECK(seg_is(&r, 3, 5, 7, 1));
    CHECK(seg_is(&r, 4, 7, 11, 3));
    CHECK(seg_is(&r, 5, 11, 16, 0));
    CHECK_EQ(r.stats[0].finish, 16);
    CHECK_EQ(r.stats[1].finish, 7);
    CHECK_EQ(r.stats[2].finish, 5);
    CHECK_EQ(r.stats[3].finish, 11);
    CHECK_EQ(turn_sum(&r), 28);
    expect_avg_weight(&r, 169, 112);
    CHECK_EQ(r.stats[0].wait, 9);

    CHECK(sched_run(&r, SCHED_HRRN, 0, 0, PRIO_NONE, jobs, 4) == 0);
    CHECK(seg_is(&r, 1, 7, 8, 2));
    CHECK(trace_has(&r, "ready 7 ratio P2=9/4 P3=4/1 P4=3/2 -> P3"));
    CHECK(trace_has(&r, "ready 8 ratio P2=5/2 P4=7/4 -> P2"));
    CHECK_EQ(turn_sum(&r), 32);
}

static void test_rr_queue_order(void) {
    SchedJob jobs[4];
    SchedJob slice[4];
    SchedResult r;
    textbook(jobs);
    CHECK(sched_run(&r, SCHED_RR, 2, 0, PRIO_NONE, jobs, 4) == 0);
    CHECK(seg_is(&r, 0, 0, 2, 0));
    CHECK(seg_is(&r, 1, 2, 4, 1));
    CHECK(seg_is(&r, 2, 4, 6, 0));
    CHECK(seg_is(&r, 3, 6, 7, 2));
    CHECK(seg_is(&r, 4, 7, 9, 1));
    CHECK(seg_is(&r, 5, 9, 11, 3));
    CHECK(seg_is(&r, 6, 11, 13, 0));
    CHECK(seg_is(&r, 7, 13, 15, 3));
    CHECK(seg_is(&r, 8, 15, 16, 0));
    CHECK(trace_has(&r, "ready 2 queue P2 P1 -> P2"));
    CHECK(trace_has(&r, "ready 4 queue P1 P3 P2 -> P1"));
    CHECK_EQ(r.stats[0].finish, 16);
    CHECK_EQ(r.stats[1].finish, 9);
    CHECK_EQ(r.stats[2].finish, 7);
    CHECK_EQ(r.stats[3].finish, 15);
    CHECK_EQ(turn_sum(&r), 36);
    expect_avg_weight(&r, 267, 112);
    CHECK_EQ(who_at(&r, 2), 1);
    CHECK_EQ(who_at(&r, 1), 0);

    slice[0] = job("P1", 0, 5, 0);
    slice[1] = job("P2", 2, 4, 0);
    slice[2] = job("P3", 4, 1, 0);
    slice[3] = job("P4", 5, 6, 0);
    CHECK(sched_run(&r, SCHED_RR, 2, 0, PRIO_NONE, slice, 4) == 0);
    CHECK(seg_is(&r, 7, 12, 16, 3));
    CHECK(trace_has(&r, "keep 14 P4"));
    CHECK_EQ(r.nseg, 8);
}

static void test_ties_and_idle(void) {
    SchedJob pair[2];
    SchedJob late[2];
    SchedResult r;
    pair[0] = job("P1", 0, 10, 0);
    pair[1] = job("P2", 0, 2, 0);
    CHECK(sched_run(&r, SCHED_HRRN, 0, 0, PRIO_NONE, pair, 2) == 0);
    CHECK(seg_is(&r, 0, 0, 10, 0));
    CHECK(trace_has(&r, "ready 0 ratio P1=1/1 P2=1/1 -> P1"));
    CHECK(sched_run(&r, SCHED_SJF, 0, 0, PRIO_NONE, pair, 2) == 0);
    CHECK(seg_is(&r, 0, 0, 2, 1));
    CHECK(seg_is(&r, 1, 2, 12, 0));

    pair[0] = job("P1", 0, 4, 0);
    pair[1] = job("P2", 2, 2, 0);
    CHECK(sched_run(&r, SCHED_SRTN, 0, 0, PRIO_NONE, pair, 2) == 0);
    CHECK(seg_is(&r, 0, 0, 4, 0));
    CHECK(seg_is(&r, 1, 4, 6, 1));

    pair[0] = job("P1", 0, 4, 5);
    pair[1] = job("P2", 1, 1, 5);
    CHECK(sched_run(&r, SCHED_PRIO_P, 0, 0, PRIO_LARGE, pair, 2) == 0);
    CHECK(seg_is(&r, 0, 0, 4, 0));
    CHECK(seg_is(&r, 1, 4, 5, 1));

    late[0] = job("P1", 0, 2, 0);
    late[1] = job("P2", 5, 3, 0);
    CHECK(sched_run(&r, SCHED_FCFS, 0, 0, PRIO_NONE, late, 2) == 0);
    CHECK(seg_is(&r, 0, 0, 2, 0));
    CHECK(seg_is(&r, 1, 2, 5, SCHED_IDLE));
    CHECK(seg_is(&r, 2, 5, 8, 1));
}

static void test_switch_admits_arrival(void) {
    SchedJob jobs[2];
    SchedResult r;
    jobs[0] = job("P1", 0, 1, 0);
    jobs[1] = job("P2", 1, 1, 0);
    CHECK(sched_run(&r, SCHED_FCFS, 0, 1, PRIO_NONE, jobs, 2) == 0);
    CHECK(seg_is(&r, 0, 0, 1, SCHED_DISPATCH));
    CHECK(seg_is(&r, 1, 1, 2, 0));
    CHECK(seg_is(&r, 2, 2, 3, SCHED_DISPATCH));
    CHECK(seg_is(&r, 3, 3, 4, 1));
    CHECK_EQ(r.stats[1].turn, 3);
    CHECK_EQ(r.stats[1].wait, 2);
}

static void test_2018_and_2023(void) {
    SchedJob y2018[3];
    SchedJob y2023[3];
    SchedResult r;
    y2018[0] = job("P1", -30, 12, 10);
    y2018[1] = job("P2", -15, 24, 30);
    y2018[2] = job("P3", -18, 36, 20);
    CHECK(sched_run(&r, SCHED_PRIO_NP, 0, 1, PRIO_LARGE, y2018, 3) == 0);
    CHECK(seg_is(&r, 0, 0, 1, SCHED_DISPATCH));
    CHECK(seg_is(&r, 1, 1, 25, 1));
    CHECK(seg_is(&r, 2, 25, 26, SCHED_DISPATCH));
    CHECK(seg_is(&r, 3, 26, 62, 2));
    CHECK(seg_is(&r, 4, 62, 63, SCHED_DISPATCH));
    CHECK(seg_is(&r, 5, 63, 75, 0));
    CHECK_EQ(r.stats[1].turn, 40);
    CHECK_EQ(r.stats[2].turn, 80);
    CHECK_EQ(r.stats[0].turn, 105);
    CHECK_EQ(turn_sum(&r), 225);
    CHECK_EQ(r.stats[1].start, 1);
    CHECK_EQ(r.stats[0].response, 93);

    y2023[0] = job("P1", 0, 60, 1);
    y2023[1] = job("P2", 20, 42, 10);
    y2023[2] = job("P3", 30, 13, 100);
    CHECK(sched_run(&r, SCHED_PRIO_P, 0, 0, PRIO_LARGE, y2023, 3) == 0);
    CHECK(seg_is(&r, 0, 0, 20, 0));
    CHECK(seg_is(&r, 1, 20, 30, 1));
    CHECK(seg_is(&r, 2, 30, 43, 2));
    CHECK(seg_is(&r, 3, 43, 75, 1));
    CHECK(seg_is(&r, 4, 75, 115, 0));
    CHECK_EQ(r.stats[0].turn, 115);
    CHECK_EQ(r.stats[1].turn, 55);
    CHECK_EQ(r.stats[2].turn, 13);
    CHECK_EQ(turn_sum(&r), 183);
    CHECK_EQ(who_at(&r, 30), 2);
    CHECK_EQ(who_at(&r, 29), 1);
    CHECK_EQ(who_at(&r, 75), 0);
    CHECK_EQ(r.stats[0].response, 0);
    CHECK_EQ(r.stats[0].wait, 55);
    CHECK_EQ(r.stats[1].wait, 13);
}

static void test_pick_2017(void) {
    SchedJob jobs[4];
    SchedResult r;
    jobs[0] = job("J1", 0, 3, 0);
    jobs[1] = job("J2", 1, 3, 0);
    jobs[2] = job("J3", 1, 2, 0);
    jobs[3] = job("J4", 3, 1, 0);
    CHECK(sched_pick(&r, SCHED_FCFS, 2, PRIO_NONE, jobs, 4) == 0);
    CHECK_EQ(r.nready, 3);
    CHECK_STR(r.jobs[r.chosen].name, "J1");
    CHECK(sched_pick(&r, SCHED_SJF, 2, PRIO_NONE, jobs, 4) == 0);
    CHECK_STR(r.jobs[r.chosen].name, "J3");
    CHECK(sched_pick(&r, SCHED_HRRN, 2, PRIO_NONE, jobs, 4) == 0);
    CHECK_STR(r.jobs[r.chosen].name, "J1");
    CHECK(sched_pick(&r, SCHED_FCFS, -1, PRIO_NONE, jobs, 4) != 0);
    CHECK_STR(r.error, "no-ready");
}

static void test_compare_turns(void) {
    SchedJob jobs[4];
    SchedResult r;
    long long expect[7];
    SchedAlgo algos[7] = {SCHED_FCFS, SCHED_SJF, SCHED_SRTN, SCHED_HRRN,
                          SCHED_RR, SCHED_PRIO_NP, SCHED_PRIO_P};
    int i;
    jobs[0] = job("P1", 0, 7, 1);
    jobs[1] = job("P2", 2, 4, 2);
    jobs[2] = job("P3", 4, 1, 3);
    jobs[3] = job("P4", 5, 4, 4);
    expect[0] = 35;
    expect[1] = 32;
    expect[2] = 28;
    expect[3] = 32;
    expect[4] = 36;
    expect[5] = 35;
    expect[6] = 30;
    for (i = 0; i < 7; i++) {
        long long q = algos[i] == SCHED_RR ? 2 : 0;
        PrioOrder order = (algos[i] == SCHED_PRIO_NP || algos[i] == SCHED_PRIO_P) ? PRIO_LARGE : PRIO_NONE;
        CHECK(sched_run(&r, algos[i], q, 0, order, jobs, 4) == 0);
        CHECK_EQ(turn_sum(&r), expect[i]);
    }
    CHECK(sched_run(&r, SCHED_PRIO_P, 0, 0, PRIO_LARGE, jobs, 4) == 0);
    expect_avg_weight(&r, 183, 112);
}

static void test_rejects(void) {
    SchedJob jobs[2];
    SchedResult r;
    jobs[0] = job("P1", 0, 1, 0);
    jobs[1] = job("P1", 1, 1, 0);
    CHECK(sched_run(&r, SCHED_FCFS, 0, 0, PRIO_NONE, jobs, 2) != 0);
    CHECK_STR(r.error, "dup-name");
    jobs[1] = job("P2", 1, 0, 0);
    CHECK(sched_run(&r, SCHED_FCFS, 0, 0, PRIO_NONE, jobs, 2) != 0);
    CHECK_STR(r.error, "bad-service");
    jobs[1] = job("P2", 1, 1, 0);
    CHECK(sched_run(&r, SCHED_RR, 2, 1, PRIO_NONE, jobs, 2) != 0);
    CHECK_STR(r.error, "bad-switch");
    CHECK(sched_run(&r, SCHED_RR, 0, 0, PRIO_NONE, jobs, 2) != 0);
    CHECK_STR(r.error, "bad-quantum");
    CHECK(sched_pick(&r, SCHED_RR, 0, PRIO_NONE, jobs, 2) != 0);
    CHECK_STR(r.error, "bad-pick");
    CHECK(sched_run(&r, SCHED_PRIO_NP, 0, 0, PRIO_NONE, jobs, 2) != 0);
    CHECK_STR(r.error, "bad-order");
}

static void test_claims_and_card(void) {
    const char *truth = NULL;
    const char *line = NULL;
    int i, n = 0;
    CHECK(exam_claim("hrrn-both", &truth) && strcmp(truth, "true") == 0);
    CHECK(exam_claim("rr-both", &truth) && strcmp(truth, "false") == 0);
    CHECK(exam_claim("sjf-both", &truth) && strcmp(truth, "false") == 0);
    CHECK(exam_claim("fcfs-both", &truth) && strcmp(truth, "false") == 0);
    CHECK(exam_claim("hrrn-short-nostarve", &truth) && strcmp(truth, "true") == 0);
    CHECK(exam_claim("sjf-short-nostarve", &truth) && strcmp(truth, "false") == 0);
    CHECK(exam_claim("rr-slice-block", &truth) && strcmp(truth, "false") == 0);
    CHECK(exam_claim("rr-finer-cost", &truth) && strcmp(truth, "true") == 0);
    CHECK(exam_claim("static-prio-starve", &truth) && strcmp(truth, "true") == 0);
    CHECK(exam_claim("sjf-always-min", &truth) && strcmp(truth, "false") == 0);
    CHECK(exam_claim("same-arrive-sjf-min", &truth) && strcmp(truth, "true") == 0);
    CHECK(exam_claim("rr-new-first", &truth) && strcmp(truth, "true") == 0);
    CHECK(!exam_claim("nope", &truth));
    CHECK(exam_row("level", "low", &line));
    CHECK_STR(line, "level low en=low queue=ready move=cpu freq=high");
    CHECK(exam_row("rule", "rr-join", &line));
    CHECK_STR(line, "rule rr-join new-then-expired");
    CHECK(exam_row("metric", "ratio", &line));
    for (i = 0; exam_card(i) != NULL; i++) {
        n++;
    }
    CHECK_EQ(n, 25);
    CHECK(exam_card(25) == NULL);
    CHECK(exam_card(-1) == NULL);
}

int main(void) {
    test_fcfs_sjf_srtn_hrrn();
    test_rr_queue_order();
    test_ties_and_idle();
    test_switch_admits_arrival();
    test_2018_and_2023();
    test_pick_2017();
    test_compare_turns();
    test_rejects();
    test_claims_and_card();
    return TEST_SUMMARY();
}
