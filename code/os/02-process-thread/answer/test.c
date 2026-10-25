#include "exam.h"
#include "forkbuf.h"
#include "pcb.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_share_and_claims(void) {
    int shared = 0;
    const char *truth = NULL;
    CHECK(exam_share("code", &shared));
    CHECK_EQ(shared, 1);
    CHECK(exam_share("open-file", &shared));
    CHECK_EQ(shared, 1);
    CHECK(exam_share("global", &shared));
    CHECK_EQ(shared, 1);
    CHECK(exam_share("stack-pointer", &shared));
    CHECK_EQ(shared, 0);
    CHECK(exam_share("stack", &shared));
    CHECK_EQ(shared, 0);
    CHECK(exam_share("fd", &shared));
    CHECK_EQ(shared, 1);
    CHECK(!exam_share("tlb", &shared));
    CHECK(exam_claim("ult-each-tcb", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("klt-os-schedule", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("kill-children", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("free-pcb", &truth));
    CHECK_STR(truth, "true");
}

static void expect_event(const char *name, const char *from, const char *to, const char *klass,
                         const char *wakeup, const char *block) {
    const EventFact *fact = NULL;
    CHECK(exam_event(name, &fact));
    CHECK_STR(fact->from, from);
    CHECK_STR(fact->to, to);
    CHECK_STR(fact->klass, klass);
    CHECK_STR(fact->wakeup, wakeup);
    CHECK_STR(fact->block, block);
}

static void test_events_match_exams(void) {
    TransFact trans;
    const ModelFact *model = NULL;
    const EntityFact *entity = NULL;
    expect_event("preempt", "running", "ready", "preempt", "no", "no");
    expect_event("p-wait", "running", "blocked", "block", "no", "yes");
    expect_event("give-cpu", "running", "ready", "preempt", "no", "no");
    expect_event("timeslice", "running", "ready", "preempt", "no", "no");
    expect_event("io-end", "blocked", "ready", "wakeup", "yes", "no");
    expect_event("leave-cs", "blocked", "ready", "wakeup", "yes", "no");
    expect_event("yield", "running", "ready", "yield", "no", "no");
    expect_event("page-fault", "running", "blocked", "block", "no", "yes");
    CHECK(exam_trans("blocked", "running", &trans));
    CHECK_EQ(trans.legal, 0);
    CHECK(exam_trans("ready", "blocked", &trans));
    CHECK_EQ(trans.legal, 0);
    CHECK(exam_trans("blocked", "ready", &trans));
    CHECK_EQ(trans.legal, 1);
    CHECK_STR(trans.how, "wakeup");
    CHECK(exam_trans("running", "ready", &trans));
    CHECK_STR(trans.how, "preempt-or-yield");
    CHECK(!exam_trans("running", "flying", &trans));
    CHECK(exam_model("many-one", &model));
    CHECK_STR(model->block_all, "yes");
    CHECK_STR(model->parallel, "no");
    CHECK(exam_model("one-one", &model));
    CHECK_STR(model->tcb, "thread");
    CHECK(exam_entity("process", &entity));
    CHECK_STR(entity->resource, "yes");
    CHECK_STR(entity->schedule, "no");
    CHECK(exam_entity("thread", &entity));
    CHECK_STR(entity->schedule, "yes");
}

static void apply(ProcTable *table, int step, const char *event, const char *pid, const char *extra,
                  PcbTrace *out) {
    CHECK(pcb_apply(table, step, event, pid, extra, out));
}

static void test_pcb_happy_path(void) {
    ProcTable table;
    PcbTrace trace;
    pcb_init(&table);
    apply(&table, 1, "create", "A", NULL, &trace);
    CHECK_EQ(trace.ok, 1);
    CHECK_STR(trace.via, "new");
    CHECK_STR(trace.ready, "A");
    CHECK_STR(trace.running, "-");
    apply(&table, 2, "create", "B", NULL, &trace);
    CHECK_STR(trace.ready, "A+B");
    apply(&table, 3, "dispatch", "A", NULL, &trace);
    CHECK_STR(trace.from, "ready");
    CHECK_STR(trace.to, "running");
    CHECK_STR(trace.ready, "B");
    CHECK_STR(trace.running, "A");
    apply(&table, 4, "preempt", "A", NULL, &trace);
    CHECK_STR(trace.ready, "B+A");
    CHECK_STR(trace.running, "-");
    apply(&table, 5, "dispatch", "B", NULL, &trace);
    apply(&table, 6, "block", "B", "disk", &trace);
    CHECK_STR(trace.to, "blocked");
    CHECK_STR(trace.ready, "A");
    CHECK_STR(trace.blocked, "disk:B");
    CHECK_STR(trace.running, "-");
    apply(&table, 7, "wakeup", "B", NULL, &trace);
    CHECK_STR(trace.from, "blocked");
    CHECK_STR(trace.to, "ready");
    CHECK_STR(trace.ready, "A+B");
    CHECK_STR(trace.blocked, "-");
    apply(&table, 8, "yield", "A", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "not-running");
    apply(&table, 9, "dispatch", "A", NULL, &trace);
    apply(&table, 10, "revoke", "B", NULL, &trace);
    CHECK_STR(trace.from, "ready");
    CHECK_STR(trace.to, "terminated");
    CHECK_STR(trace.ready, "-");
    CHECK_STR(trace.running, "A");
    apply(&table, 11, "exit", "A", NULL, &trace);
    CHECK_EQ(trace.ok, 1);
    CHECK_STR(trace.running, "-");
    apply(&table, 12, "create", "A", NULL, &trace);
    CHECK_EQ(trace.ok, 1);
    CHECK_STR(trace.ready, "A");
}

static void test_pcb_rejects(void) {
    ProcTable table;
    PcbTrace trace;
    char text[512];
    pcb_init(&table);
    apply(&table, 1, "create", "A", NULL, &trace);
    apply(&table, 2, "create", "B", NULL, &trace);
    apply(&table, 3, "dispatch", "A", NULL, &trace);
    apply(&table, 4, "block", "A", "io", &trace);
    apply(&table, 5, "dispatch", "A", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "not-ready");
    CHECK_STR(trace.from, "blocked");
    CHECK_STR(trace.to, "running");
    CHECK_STR(trace.blocked, "io:A");
    apply(&table, 6, "dispatch", "B", NULL, &trace);
    CHECK_EQ(trace.ok, 1);
    apply(&table, 7, "dispatch", "B", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "not-ready");
    apply(&table, 8, "create", "C", NULL, &trace);
    apply(&table, 9, "dispatch", "C", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "cpu-busy");
    CHECK_STR(trace.running, "B");
    apply(&table, 10, "exit", "C", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "not-running");
    CHECK(!pcb_apply(&table, 11, "nope", "A", NULL, &trace));
    apply(&table, 12, "create", "B", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "exists");
    pcb_format(&trace, text, sizeof text);
    CHECK(strstr(text, "ok=no") != NULL);
    CHECK(strstr(text, "why=exists") != NULL);
}

static void test_two_block_queues(void) {
    ProcTable table;
    PcbTrace trace;
    pcb_init(&table);
    apply(&table, 1, "create", "A", NULL, &trace);
    apply(&table, 2, "create", "B", NULL, &trace);
    apply(&table, 3, "create", "C", NULL, &trace);
    apply(&table, 4, "dispatch", "A", NULL, &trace);
    apply(&table, 5, "block", "A", "io", &trace);
    apply(&table, 6, "dispatch", "B", NULL, &trace);
    apply(&table, 7, "block", "B", "io", &trace);
    CHECK_STR(trace.blocked, "io:A+B");
    apply(&table, 8, "dispatch", "C", NULL, &trace);
    apply(&table, 9, "block", "C", "disk", &trace);
    CHECK_STR(trace.blocked, "io:A+B,disk:C");
    CHECK_STR(trace.running, "-");
    apply(&table, 10, "wakeup", "A", NULL, &trace);
    CHECK_STR(trace.ready, "A");
    CHECK_STR(trace.blocked, "io:B,disk:C");
    apply(&table, 11, "revoke", "B", NULL, &trace);
    CHECK_STR(trace.blocked, "disk:C");
    CHECK_STR(trace.ready, "A");
}

static void test_table_full(void) {
    ProcTable table;
    PcbTrace trace;
    int i;
    char name[8];
    pcb_init(&table);
    for (i = 0; i < PCB_MAX; i++) {
        snprintf(name, sizeof name, "P%d", i);
        apply(&table, i + 1, "create", name, NULL, &trace);
        CHECK_EQ(trace.ok, 1);
    }
    apply(&table, PCB_MAX + 1, "create", "PX", NULL, &trace);
    CHECK_EQ(trace.ok, 0);
    CHECK_STR(trace.why, "full");
    CHECK_STR(trace.ready, "P0+P1+P2+P3+P4+P5+P6+P7+P8+P9+P10+P11+P12+P13+P14+P15");
}

static void test_fork_buffer(void) {
    int chars = 0;
    int procs = 0;
    CHECK(forkbuf_count("nolf", &chars, &procs));
    CHECK_EQ(chars, 8);
    CHECK_EQ(procs, 4);
    CHECK(forkbuf_count("line", &chars, &procs));
    CHECK_EQ(chars, 6);
    CHECK_EQ(procs, 4);
    CHECK(forkbuf_count("flush", &chars, &procs));
    CHECK_EQ(chars, 6);
    CHECK(!forkbuf_count("tty", &chars, &procs));
}

int main(void) {
    test_share_and_claims();
    test_events_match_exams();
    test_pcb_happy_path();
    test_pcb_rejects();
    test_two_block_queues();
    test_table_full();
    test_fork_buffer();
    return TEST_SUMMARY();
}
