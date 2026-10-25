#include "exam.h"
#include "hold.h"
#include "mlfq.h"
#include "smp.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static MlfqJob mj(const char *name, long long arrive, long long cpu1, long long io, long long cpu2) {
    MlfqJob job;
    memset(&job, 0, sizeof job);
    snprintf(job.name, sizeof job.name, "%s", name);
    job.arrive = arrive;
    job.cpu1 = cpu1;
    job.io = io;
    job.cpu2 = cpu2;
    return job;
}

static SmpJob sj(const char *name, long long arrive, long long service, int affinity) {
    SmpJob job;
    memset(&job, 0, sizeof job);
    snprintf(job.name, sizeof job.name, "%s", name);
    job.arrive = arrive;
    job.service = service;
    job.affinity = affinity;
    return job;
}

static int trace_has(const MlfqResult *result, const char *piece) {
    int i;
    for (i = 0; i < result->ntrace; i++) {
        if (strstr(result->trace[i], piece) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int seg_is(const MlfqResult *result, int index, long long t0, long long t1, const char *name) {
    if (index < 0 || index >= result->nseg) {
        return 0;
    }
    return result->gt0[index] == t0 && result->gt1[index] == t1 &&
           strcmp(result->gname[index], name) == 0;
}

static void test_feedback_numbers(void) {
    MlfqSpec spec;
    MlfqJob jobs[3];
    MlfqResult result;
    CHECK(mlfq_standard(&spec, 3, 1) == 0);
    jobs[0] = mj("P1", 0, 4, 0, 0);
    jobs[1] = mj("P2", 1, 2, 0, 0);
    jobs[2] = mj("P3", 3, 1, 0, 0);
    CHECK(mlfq_run(&spec, jobs, 3, &result) == 0);
    CHECK_EQ(result.stats[0].finish, 7);
    CHECK_EQ(result.stats[1].finish, 5);
    CHECK_EQ(result.stats[2].finish, 4);
    CHECK_EQ(result.stats[0].wait, 3);
    CHECK_EQ(result.stats[1].wait, 2);
    CHECK_EQ(result.stats[2].wait, 0);
    CHECK(trace_has(&result, "preempt P1 stay q1"));
    CHECK(seg_is(&result, 0, 0, 1, "P1"));
    CHECK(seg_is(&result, 5, 5, 7, "P1"));
}

static void test_2019(void) {
    MlfqSpec spec;
    MlfqJob jobs[2];
    MlfqResult result;
    mlfq_fb2(&spec);
    jobs[0] = mj("P1", 0, 30, 0, 0);
    jobs[1] = mj("P2", 0, 20, 0, 0);
    CHECK(mlfq_run(&spec, jobs, 2, &result) == 0);
    CHECK_EQ(result.stats[0].finish, 50);
    CHECK_EQ(result.stats[1].finish, 30);
    CHECK_EQ(result.stats[0].wait, 20);
    CHECK_EQ(result.stats[1].wait, 10);
    CHECK_EQ(result.stats[1].response, 10);
    CHECK(seg_is(&result, 0, 0, 10, "P1"));
    CHECK(seg_is(&result, 1, 10, 30, "P2"));
    CHECK(seg_is(&result, 2, 30, 50, "P1"));
}

static void test_idle_block_and_same_queue(void) {
    MlfqSpec spec;
    MlfqJob jobs[2];
    MlfqResult result;
    CHECK(mlfq_standard(&spec, 1, 1) == 0);
    jobs[0] = mj("P1", 0, 1, 0, 0);
    jobs[1] = mj("P2", 3, 1, 0, 0);
    CHECK(mlfq_run(&spec, jobs, 2, &result) == 0);
    CHECK(seg_is(&result, 1, 1, 3, "idle"));
    CHECK_EQ(result.stats[1].finish, 4);
    jobs[0] = mj("P1", 0, 1, 2, 1);
    jobs[1] = mj("P2", 0, 2, 0, 0);
    CHECK(mlfq_run(&spec, jobs, 2, &result) == 0);
    CHECK_EQ(result.stats[0].wait, 0);
    CHECK_EQ(result.stats[1].wait, 1);
    CHECK_EQ(result.stats[0].finish, 4);
    CHECK(trace_has(&result, "switch=no"));
    CHECK(mlfq_standard(&spec, 1, 4) == 0);
    jobs[0] = mj("P1", 0, 3, 0, 0);
    jobs[1] = mj("P2", 1, 1, 0, 0);
    CHECK(mlfq_run(&spec, jobs, 2, &result) == 0);
    CHECK_EQ(result.stats[0].finish, 3);
    CHECK_EQ(result.stats[1].finish, 4);
    CHECK(mlfq_standard(&spec, 0, 1) == -1);
    CHECK(mlfq_standard(&spec, 2, 0) == -2);
}

static void test_smp_numbers(void) {
    SmpJob jobs[3];
    SmpResult result;
    jobs[0] = sj("A", 0, 4, -1);
    jobs[1] = sj("B", 0, 4, -1);
    jobs[2] = sj("C", 0, 4, -1);
    CHECK(smp_run(1, 0, SMP_NONE, 2, jobs, 3, &result) == 0);
    CHECK_EQ(result.migrations, 3);
    CHECK_EQ(result.makespan, 6);
    CHECK(smp_run(0, 0, SMP_NONE, 2, jobs, 3, &result) == 0);
    CHECK_EQ(result.migrations, 0);
    CHECK_EQ(result.makespan, 8);
    CHECK(smp_run(0, 1, SMP_NONE, 2, jobs, 3, &result) == 0);
    CHECK_EQ(result.migrations, 1);
    CHECK_EQ(result.makespan, 6);
    jobs[0] = sj("A", 0, 4, 0);
    jobs[1] = sj("B", 0, 2, 0);
    CHECK(smp_run(1, 0, SMP_HARD, 2, jobs, 2, &result) == 0);
    CHECK_EQ(result.migrations, 0);
    CHECK_EQ(result.makespan, 6);
    CHECK(smp_run(1, 0, SMP_SOFT, 2, jobs, 2, &result) == 0);
    CHECK_EQ(result.migrations, 1);
    CHECK_EQ(result.makespan, 4);
}

static void test_hold_and_claims(void) {
    Hold hold;
    const char *truth = NULL;
    CHECK(hold_decide("interrupt", "exit", &hold) == 0);
    CHECK_EQ(hold.want, 1);
    CHECK_EQ(hold.defer, 1);
    CHECK(hold_decide("user-cs", "slice", &hold) == 0);
    CHECK_EQ(hold.defer, 0);
    CHECK(hold_decide("interrupt", "return", &hold) == 0);
    CHECK_EQ(hold.defer, 0);
    CHECK(hold_decide("kernel-cs", "higher", &hold) == 0);
    CHECK_EQ(hold.defer, 1);
    CHECK(hold_decide("atomic", "block", &hold) == 0);
    CHECK_EQ(hold.defer, 1);
    CHECK(hold_decide("user-cs", "nope", &hold) != 0);
    CHECK(exam_claim("user-cs-cannot", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("exit-can", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("switch-pc", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("switch-stack", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("switch-ptbr", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("switch-only-ptbr", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("prio-insert-on", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("prio-select-o1", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("prio-insert-o1", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("ult-each-tcb", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("klt-os-schedule", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("slice-always-switch", &truth));
    CHECK_STR(truth, "false");
    CHECK(!exam_claim("not-a-claim", &truth));
}

int main(void) {
    test_feedback_numbers();
    test_2019();
    test_idle_block_and_same_queue();
    test_smp_numbers();
    test_hold_and_claims();
    return TEST_SUMMARY();
}
