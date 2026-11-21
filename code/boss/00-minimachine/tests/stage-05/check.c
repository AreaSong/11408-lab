#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_exec.h"
#include "kernel/k_proc.h"
#include "kernel/k_sched.h"
#include "kernel/k_trap.h"
#include "test.h"

#include <string.h>

static void job_set(KJob *job, const char *name, long long arrive, long long service, long long prio) {
    memset(job, 0, sizeof *job);
    snprintf(job->name, sizeof job->name, "%s", name);
    job->arrive = arrive;
    job->service = service;
    job->prio = prio;
}

static void wang_four(KJob *jobs) {
    job_set(&jobs[0], "P1", 0, 7, 0);
    job_set(&jobs[1], "P2", 2, 4, 0);
    job_set(&jobs[2], "P3", 4, 1, 0);
    job_set(&jobs[3], "P4", 5, 4, 0);
}

static int seg_is(const KSchedOut *out, int i, long long t0, long long t1, const char *name) {
    return i < out->nseg && out->seg_t0[i] == t0 && out->seg_t1[i] == t1 && strcmp(out->seg_name[i], name) == 0;
}

static void test_fcfs_sjf_srtn(void) {
    KJob jobs[4];
    KSchedOut out;
    wang_four(jobs);
    CHECK(k_sched_run(&out, K_FCFS, 0, 0, 1, jobs, 4) == 0);
    CHECK_EQ(out.avg_turn_n, 35);
    CHECK_EQ(out.avg_turn_d, 4);
    CHECK_EQ(out.avg_weight_n, 7);
    CHECK_EQ(out.avg_weight_d, 2);
    CHECK_EQ(out.avg_wait_n, 19);
    CHECK_EQ(out.avg_wait_d, 4);
    CHECK(seg_is(&out, 0, 0, 7, "P1"));
    CHECK(seg_is(&out, 3, 12, 16, "P4"));
    CHECK(k_sched_run(&out, K_SJF, 0, 0, 1, jobs, 4) == 0);
    CHECK_EQ(out.avg_turn_n, 8);
    CHECK_EQ(out.avg_turn_d, 1);
    CHECK_EQ(out.avg_weight_n, 41);
    CHECK_EQ(out.avg_weight_d, 16);
    CHECK_EQ(out.avg_wait_n, 4);
    CHECK_EQ(out.avg_wait_d, 1);
    CHECK(seg_is(&out, 1, 7, 8, "P3"));
    CHECK(k_sched_run(&out, K_SRTN, 0, 0, 1, jobs, 4) == 0);
    CHECK_EQ(out.avg_turn_n, 7);
    CHECK_EQ(out.avg_turn_d, 1);
    CHECK_EQ(out.avg_wait_n, 3);
    CHECK_EQ(out.avg_wait_d, 1);
    CHECK_EQ(out.avg_weight_n, 169);
    CHECK_EQ(out.avg_weight_d, 112);
    CHECK(seg_is(&out, 0, 0, 2, "P1"));
    CHECK(seg_is(&out, 2, 4, 5, "P3"));
}

static void test_rr_hrrn_prio(void) {
    KJob jobs[4];
    KSchedOut out;
    const char *how = NULL;
    wang_four(jobs);
    CHECK(k_sched_run(&out, K_RR, 2, 0, 1, jobs, 4) == 0);
    CHECK_EQ(out.finish[0], 16);
    CHECK_EQ(out.finish[1], 9);
    CHECK_EQ(out.finish[2], 7);
    CHECK_EQ(out.finish[3], 15);
    CHECK_EQ(out.avg_turn_n, 9);
    CHECK_EQ(out.avg_turn_d, 1);
    CHECK(seg_is(&out, 1, 2, 4, "P2"));
    CHECK(k_sched_run(&out, K_HRRN, 0, 0, 1, jobs, 4) == 0);
    CHECK(seg_is(&out, 1, 7, 8, "P3"));
    job_set(&jobs[0], "P1", 0, 10, 0);
    job_set(&jobs[1], "P2", 0, 2, 0);
    CHECK(k_sched_run(&out, K_HRRN, 0, 0, 1, jobs, 2) == 0);
    CHECK(seg_is(&out, 0, 0, 10, "P1"));
    CHECK(k_sched_run(&out, K_SJF, 0, 0, 1, jobs, 2) == 0);
    CHECK(seg_is(&out, 0, 0, 2, "P2"));
    job_set(&jobs[0], "P1", 0, 60, 1);
    job_set(&jobs[1], "P2", 20, 42, 10);
    job_set(&jobs[2], "P3", 30, 13, 100);
    CHECK(k_sched_run(&out, K_PRIO_P, 0, 0, 1, jobs, 3) == 0);
    CHECK_EQ(out.avg_turn_n, 61);
    CHECK_EQ(out.avg_turn_d, 1);
    CHECK_EQ(out.finish[2], 43);
    CHECK_EQ(out.wait[0], 55);
    CHECK_EQ(k_when(K_RR, K_WHEN_SLICE), 1);
    CHECK_EQ(k_when(K_FCFS, K_WHEN_SLICE), 0);
    CHECK_EQ(k_when(K_SRTN, K_WHEN_ARRIVE), 1);
    CHECK_EQ(k_when(K_FCFS, K_WHEN_EXIT), 1);
    CHECK_EQ(k_when(K_SJF, K_WHEN_ARRIVE), 0);
    CHECK(k_trans_ok("running", "blocked", &how) == 1);
    CHECK_STR(how, "block");
    CHECK(k_trans_ok("ready", "blocked", &how) == 0);
    CHECK_STR(how, "-");
    CHECK(k_trans_ok("blocked", "running", &how) == 0);
    CHECK(k_trans_ok("running", "ready", &how) == 1);
    CHECK_EQ(k_ctx_count(), 11);
    CHECK_STR(k_ctx_field(8), "pc");
    CHECK_STR(k_ctx_field(10), "ptbr");
}

static void test_mlfq(void) {
    KJob jobs[3];
    KSchedOut out;
    job_set(&jobs[0], "P1", 0, 4, 0);
    job_set(&jobs[1], "P2", 1, 2, 0);
    job_set(&jobs[2], "P3", 3, 1, 0);
    CHECK(k_sched_run(&out, K_MLFQ, 1, 3, 1, jobs, 3) == 0);
    CHECK_EQ(out.finish[0], 7);
    CHECK_EQ(out.finish[1], 5);
    CHECK_EQ(out.finish[2], 4);
    CHECK_EQ(out.avg_wait_n, 5);
    CHECK_EQ(out.avg_wait_d, 3);
    CHECK(seg_is(&out, 0, 0, 1, "P1"));
    CHECK(seg_is(&out, 3, 3, 4, "P3"));
    job_set(&jobs[0], "P1", 0, 30, 0);
    job_set(&jobs[1], "P2", 0, 20, 0);
    CHECK(k_sched_run(&out, K_FB2, 0, 0, 1, jobs, 2) == 0);
    CHECK_EQ(out.avg_wait_n, 15);
    CHECK_EQ(out.avg_wait_d, 1);
    CHECK(seg_is(&out, 0, 0, 10, "P1"));
    CHECK(seg_is(&out, 1, 10, 30, "P2"));
    CHECK(seg_is(&out, 2, 30, 50, "P1"));
}

static int load_src(MmMachine *m, const char *src) {
    AsmImage *img = NULL;
    char err[256];
    if (asm_assemble(src, &img, err, sizeof err) != 0) {
        return -1;
    }
    mm_reset(m);
    if (mm_load(m, img) != 0) {
        asm_free(img);
        return -1;
    }
    asm_free(img);
    return k_boot(m) == 0 && k_proc_on(m) == 0 ? 0 : -1;
}

static void test_fork_and_exit(void) {
    static const char *src =
        "        .text\n"
        "        ld    r0, #3\n"
        "        syscall\n"
        "        ld    r0, #3\n"
        "        syscall\n"
        "        ld    r0, #1\n"
        "        ld    r1, #1\n"
        "        la    r2, msg\n"
        "        ld    r3, #1\n"
        "        syscall\n"
        "        ld    r0, #0\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        .data\n"
        "msg:    .byte 65\n";
    MmMachine *m = mm_create(0);
    char buf[32];
    int state = -1;
    uint32_t pt1 = 0;
    uint32_t pt2 = 0;
    CHECK(load_src(m, src) == 0);
    CHECK_EQ(cpu_run(m, 8000), 0);
    CHECK(k_exited(m, NULL));
    k_console_out(m, buf, (int)sizeof buf);
    CHECK_STR(buf, "AAAA");
    CHECK(k_switches(m) >= 3);
    CHECK(k_pcb(m, 1, &state, &pt1) == 0);
    CHECK(k_pcb(m, 2, &state, &pt2) == 0);
    CHECK(pt1 != pt2);
    CHECK(pt1 != 0);
    mm_destroy(m);
}

static void test_getpid_sleep_slice(void) {
    static const char *pid_src =
        "        .text\n"
        "        ld    r0, #6\n"
        "        syscall\n"
        "        mov   r1, r0\n"
        "        ld    r0, #0\n"
        "        syscall\n";
    static const char *sleep_src =
        "        .text\n"
        "        ld    r0, #7\n"
        "        ld    r1, #40\n"
        "        syscall\n"
        "        ld    r0, #1\n"
        "        ld    r1, #1\n"
        "        la    r2, msg\n"
        "        ld    r3, #1\n"
        "        syscall\n"
        "        ld    r0, #0\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        .data\n"
        "msg:    .byte 90\n";
    static const char *slice_src =
        "        .text\n"
        "        nop\n"
        "        nop\n"
        "        nop\n"
        "        nop\n"
        "        ld    r0, #0\n"
        "        ld    r1, #3\n"
        "        syscall\n";
    MmMachine *m = mm_create(0);
    char buf[16];
    int code = -1;
    CHECK(load_src(m, pid_src) == 0);
    CHECK_EQ(cpu_run(m, 200), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 1);
    mm_destroy(m);
    m = mm_create(0);
    CHECK(load_src(m, sleep_src) == 0);
    CHECK_EQ(cpu_run(m, 2000), 0);
    k_console_out(m, buf, (int)sizeof buf);
    CHECK_STR(buf, "Z");
    CHECK(k_switches(m) >= 1);
    mm_destroy(m);
    m = mm_create(0);
    CHECK(load_src(m, slice_src) == 0);
    CHECK(k_proc_policy(m, K_RR, 1, 1) == 0);
    CHECK_EQ(cpu_run(m, 200), 0);
    CHECK(k_slices(m) > 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 3);
    mm_destroy(m);
}

int main(void) {
    test_fcfs_sjf_srtn();
    test_rr_hrrn_prio();
    test_mlfq();
    test_fork_and_exit();
    test_getpid_sleep_slice();
    return TEST_SUMMARY();
}
