#include "exam.h"
#include "logic.h"
#include "monitor.h"
#include "peterson.h"
#include "rule.h"
#include "sem.h"
#include "soft.h"
#include "test.h"
#include "trial.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>

static void test_rules(void) {
    CHECK_STR(rule_line(ALGO_SINGLE), "algo=single progress=no mutex=yes bounded=yes yield=no");
    CHECK_STR(rule_line(ALGO_CHECK_FIRST), "algo=check-first progress=yes mutex=no bounded=no yield=no");
    CHECK_STR(rule_line(ALGO_CHECK_AFTER), "algo=check-after progress=no mutex=yes bounded=no yield=no");
    CHECK_STR(rule_line(ALGO_PETERSON), "algo=peterson progress=yes mutex=yes bounded=yes yield=no");
    CHECK_STR(rule_line(ALGO_TSL), "algo=tsl progress=yes mutex=yes bounded=no yield=no");
    CHECK_STR(rule_line(ALGO_RECORD), "algo=record progress=yes mutex=yes bounded=yes yield=yes");
    CHECK_EQ(rule_id("swap"), ALGO_SWAP);
    CHECK(!rule_can_step(ALGO_INTEGER));
    CHECK(rule_can_step(ALGO_PETERSON));
}

static void test_check_first_both(void) {
    Soft state;
    SoftView view;
    soft_init(&state);
    CHECK_EQ(soft_step(&state, ALGO_CHECK_FIRST, 0, 0, &view), 0);
    CHECK_STR(view.action, "pass");
    CHECK_EQ(soft_step(&state, ALGO_CHECK_FIRST, 1, 0, &view), 0);
    CHECK_EQ(soft_step(&state, ALGO_CHECK_FIRST, 0, 0, &view), 0);
    CHECK_EQ(soft_step(&state, ALGO_CHECK_FIRST, 1, 0, &view), 0);
    CHECK_STR(view.inside, "both");
}

static void test_check_after_stuck(void) {
    Soft state;
    SoftView view;
    soft_init(&state);
    CHECK_EQ(soft_step(&state, ALGO_CHECK_AFTER, 0, 0, &view), 0);
    CHECK_EQ(soft_step(&state, ALGO_CHECK_AFTER, 1, 0, &view), 0);
    CHECK_EQ(soft_step(&state, ALGO_CHECK_AFTER, 0, 0, &view), 0);
    CHECK_STR(view.action, "spin");
    CHECK_EQ(soft_step(&state, ALGO_CHECK_AFTER, 1, 0, &view), 0);
    CHECK_STR(view.inside, "-");
}

static void test_peterson_tie(void) {
    Soft state;
    SoftView view;
    int who[] = {0, 1, 1, 0, 0, 1};
    int i;
    soft_init(&state);
    for (i = 0; i < 6; i++) {
        CHECK_EQ(soft_step(&state, ALGO_PETERSON, who[i], 0, &view), 0);
    }
    CHECK_STR(view.action, "enter");
    CHECK_STR(view.inside, "1");
    CHECK_EQ(view.turn, 1);
}

static void test_single_idle(void) {
    Soft state;
    SoftView view;
    soft_init(&state);
    CHECK_EQ(soft_step(&state, ALGO_SINGLE, 0, 0, &view), 0);
    CHECK_STR(view.action, "enter");
    CHECK_EQ(soft_step(&state, ALGO_SINGLE, 0, 1, &view), 0);
    CHECK_EQ(view.turn, 1);
    CHECK_EQ(soft_step(&state, ALGO_SINGLE, 0, 0, &view), 0);
    CHECK_STR(view.action, "spin");
    CHECK_STR(view.inside, "-");
    CHECK_EQ(soft_step(&state, ALGO_SINGLE, 0, 1, &view), -2);
}

static void test_tsl_spin(void) {
    Soft state;
    SoftView view;
    soft_init(&state);
    CHECK_EQ(soft_step(&state, ALGO_TSL, 0, 0, &view), 0);
    CHECK_EQ(soft_step(&state, ALGO_TSL, 1, 0, &view), 0);
    CHECK_STR(view.action, "spin");
    CHECK_EQ(view.lock, 1);
    CHECK_EQ(soft_step(&state, ALGO_TSL, 0, 1, &view), 0);
    CHECK_EQ(soft_step(&state, ALGO_TSL, 1, 0, &view), 0);
    CHECK_STR(view.action, "enter");
    CHECK_EQ(soft_step(&state, ALGO_TSL, 1, 0, &view), -3);
}

static void test_sem_value(void) {
    LogicSem sem;
    PvView view;
    logic_sem_init(&sem, 3);
    CHECK_EQ(logic_p(&sem, "A", &view), 0);
    CHECK_EQ(logic_p(&sem, "B", &view), 0);
    CHECK_EQ(view.value, 1);
    CHECK_EQ(view.available, 1);
    CHECK_EQ(view.waiters, 0);
    CHECK_EQ(logic_available(1), 1);
    CHECK_EQ(logic_waiters(1), 0);
    CHECK_EQ(logic_available(-3), 0);
    CHECK_EQ(logic_waiters(-3), 3);
    CHECK_EQ(logic_available(0), 0);
    CHECK_EQ(logic_waiters(0), 0);
}

static void test_sem_fifo(void) {
    LogicSem sem;
    PvView view;
    logic_sem_init(&sem, 1);
    CHECK_EQ(logic_p(&sem, "A", &view), 0);
    CHECK_STR(view.state, "run");
    CHECK_EQ(logic_p(&sem, "B", &view), 0);
    CHECK_EQ(logic_p(&sem, "C", &view), 0);
    CHECK_STR(view.queue, "B,C");
    CHECK_EQ(view.value, -2);
    CHECK_EQ(logic_p(&sem, "B", &view), -2);
    CHECK_EQ(logic_v(&sem, "A", &view), 0);
    CHECK_STR(view.state, "wake");
    CHECK_STR(view.who, "B");
    CHECK_STR(view.queue, "C");
    CHECK_EQ(view.waiters, 1);
    CHECK_EQ(logic_p(&sem, "9bad", &view), -1);
}

static void test_cond_lost(void) {
    LogicBox box;
    CondView view;
    logic_box_init(&box);
    CHECK_EQ(logic_wait(&box, "A", "empty", &view), 0);
    CHECK_EQ(logic_wait(&box, "B", "empty", &view), 0);
    CHECK_EQ(logic_signal(&box, "C", "empty", &view), 0);
    CHECK_STR(view.wake, "A");
    CHECK_STR(view.queue, "B");
    CHECK_STR(view.state, "wake");
    CHECK_EQ(logic_signal(&box, "D", "empty", &view), 0);
    CHECK_STR(view.wake, "B");
    CHECK_EQ(logic_signal(&box, "E", "full", &view), 0);
    CHECK_STR(view.state, "lost");
    CHECK_STR(view.wake, "-");
}

static void test_claims(void) {
    const char *truth = NULL;
    const char *line = NULL;
    CHECK(exam_claim("peterson-mutex", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("peterson-starve", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("yield-sem", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("yield-integer", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("tsl-no-yield-cpu", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("tsl-yield-wait", &truth));
    CHECK_STR(truth, "false");
    CHECK(exam_claim("cond-wait-block", &truth));
    CHECK_STR(truth, "true");
    CHECK(exam_claim("signal-kept", &truth));
    CHECK_STR(truth, "false");
    CHECK(!exam_claim("no-such", &truth));
    CHECK(exam_row("sem", "negative", &line));
    CHECK_STR(line, "sem negative available=0 waiters=abs");
    CHECK(exam_row("principle", "yield", &line));
    CHECK_STR(line, "principle yield block-not-spin");
}

typedef struct {
    Sem *sem;
    atomic_int *passed;
} BlockArg;

static void *blocker(void *arg) {
    BlockArg *a = arg;
    sem_P(a->sem);
    atomic_store(a->passed, 1);
    return NULL;
}

static void test_real_sem(void) {
    Sem sem;
    atomic_int passed;
    BlockArg arg;
    pthread_t thread;
    int value = 0;
    int waiting = 0;
    int i;
    CHECK_EQ(sem_init(&sem, 0), 0);
    atomic_init(&passed, 0);
    arg.sem = &sem;
    arg.passed = &passed;
    CHECK_EQ(pthread_create(&thread, NULL, blocker, &arg), 0);
    for (i = 0; i < 1000000; i++) {
        sem_snapshot(&sem, &value, &waiting);
        if (waiting == 1 && value == -1) {
            break;
        }
        sched_yield();
    }
    CHECK_EQ(waiting, 1);
    CHECK_EQ(value, -1);
    CHECK_EQ(atomic_load(&passed), 0);
    sem_V(&sem);
    pthread_join(thread, NULL);
    sem_snapshot(&sem, &value, &waiting);
    CHECK_EQ(value, 0);
    CHECK_EQ(waiting, 0);
    CHECK_EQ(atomic_load(&passed), 1);
    sem_destroy(&sem);
}

static void test_locks_and_fence(void) {
    int got = 0;
    int bad = 0;
    CHECK_EQ(trial_lock(TRIAL_TSL, 800, 0, &got), 0);
    CHECK_EQ(got, 1600);
    CHECK_EQ(trial_lock(TRIAL_SWAP, 800, 0, &got), 0);
    CHECK_EQ(got, 1600);
    CHECK_EQ(trial_lock(TRIAL_MUTEX, 400, 0, &got), 0);
    CHECK_EQ(got, 800);
    CHECK_EQ(trial_lock(TRIAL_SEM, 400, 0, &got), 0);
    CHECK_EQ(got, 800);
    CHECK_EQ(peterson_run(1, 1500, &got, &bad), 0);
    CHECK_EQ(got, 3000);
    CHECK_EQ(bad, 0);
}

typedef struct {
    Monitor *monitor;
    int n;
    int *out;
} MonArg;

static void *producer(void *arg) {
    MonArg *a = arg;
    int i;
    for (i = 1; i <= a->n; i++) {
        monitor_put(a->monitor, i);
    }
    return NULL;
}

static void *consumer(void *arg) {
    MonArg *a = arg;
    int i;
    for (i = 0; i < a->n; i++) {
        a->out[i] = monitor_get(a->monitor);
    }
    return NULL;
}

static void test_monitor(void) {
    Monitor monitor;
    int out[20];
    MonArg arg;
    pthread_t put_thread;
    pthread_t get_thread;
    int i;
    CHECK_EQ(monitor_init(&monitor, 2), 0);
    monitor_put(&monitor, 7);
    CHECK_EQ(monitor_get(&monitor), 7);
    arg.monitor = &monitor;
    arg.n = 20;
    arg.out = out;
    CHECK_EQ(pthread_create(&put_thread, NULL, producer, &arg), 0);
    CHECK_EQ(pthread_create(&get_thread, NULL, consumer, &arg), 0);
    pthread_join(put_thread, NULL);
    pthread_join(get_thread, NULL);
    for (i = 0; i < 20; i++) {
        CHECK_EQ(out[i], i + 1);
    }
    monitor_destroy(&monitor);
    CHECK_EQ(monitor_init(&monitor, 0), -1);
}

int main(void) {
    test_rules();
    test_check_first_both();
    test_check_after_stuck();
    test_peterson_tie();
    test_single_idle();
    test_tsl_spin();
    test_sem_value();
    test_sem_fifo();
    test_cond_lost();
    test_claims();
    test_real_sem();
    test_locks_and_fence();
    test_monitor();
    return TEST_SUMMARY();
}
