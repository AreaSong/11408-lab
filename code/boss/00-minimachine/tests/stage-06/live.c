#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_exec.h"
#include "kernel/k_proc.h"
#include "kernel/k_sched.h"
#include "kernel/k_sync.h"
#include "kernel/k_trap.h"
#include "test.h"

#include <stdio.h>

static int boot(MmMachine *m, const char *src) {
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

static void test_sem_block(void) {
    static const char *src =
        "        .text\n"
        "        ld    r0, #8\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        mov   r4, r0\n"
        "        ld    r0, #3\n"
        "        syscall\n"
        "        cmpi  r0, #0\n"
        "        jne   parent\n"
        "        ld    r0, #10\n"
        "        mov   r1, r4\n"
        "        syscall\n"
        "        ld    r0, #0\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "parent: ld    r0, #9\n"
        "        mov   r1, r4\n"
        "        syscall\n"
        "        ld    r0, #0\n"
        "        ld    r1, #1\n"
        "        syscall\n";
    MmMachine *m = mm_create(0);
    int state = -1;
    int value = 0;
    int nwait = 0;
    int code = -1;
    CHECK(boot(m, src) == 0);
    CHECK_EQ(cpu_run(m, 4000), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 1);
    CHECK(k_sem_stat(m, 0, &value, &nwait) == 0);
    CHECK_EQ(value, 0);
    CHECK_EQ(nwait, 0);
    CHECK(k_sem_ok(value, nwait));
    CHECK(k_pcb(m, 1, &state, NULL) == 0);
    CHECK_EQ(state, K_ST_TERM);
    mm_destroy(m);
}

static void test_pc(void) {
    static const char *pc =
        "        .text\n"
        "        la    r2, meta\n"
        "        ld    r0, #8\n"
        "        ld    r1, #1\n"
        "        syscall\n"
        "        st    r0, [r2]\n"
        "        addi  r2, r2, #4\n"
        "        ld    r0, #8\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        st    r0, [r2]\n"
        "        addi  r2, r2, #4\n"
        "        ld    r0, #8\n"
        "        ld    r1, #1\n"
        "        syscall\n"
        "        st    r0, [r2]\n"
        "        ld    r0, #3\n"
        "        syscall\n"
        "        mov   r3, r0\n"
        "        cmpi  r3, #0\n"
        "        jne   parent\n"
        "        la    r4, meta\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #9\n"
        "        syscall\n"
        "        la    r4, meta\n"
        "        addi  r4, r4, #8\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #9\n"
        "        syscall\n"
        "        la    r2, slot\n"
        "        ld    r1, #7\n"
        "        st    r1, [r2]\n"
        "        la    r4, meta\n"
        "        addi  r4, r4, #8\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #10\n"
        "        syscall\n"
        "        la    r4, meta\n"
        "        addi  r4, r4, #4\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #10\n"
        "        syscall\n"
        "        ld    r0, #0\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "parent: la    r4, meta\n"
        "        addi  r4, r4, #4\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #9\n"
        "        syscall\n"
        "        la    r4, meta\n"
        "        addi  r4, r4, #8\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #9\n"
        "        syscall\n"
        "        la    r2, slot\n"
        "        ld    r5, [r2]\n"
        "        la    r4, meta\n"
        "        addi  r4, r4, #8\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #10\n"
        "        syscall\n"
        "        la    r4, meta\n"
        "        ld    r1, [r4]\n"
        "        ld    r0, #10\n"
        "        syscall\n"
        "        ld    r0, #5\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        mov   r1, r5\n"
        "        ld    r0, #0\n"
        "        syscall\n"
        "        .org  0x2000\n"
        "slot:   .word 0\n"
        "meta:   .word 0\n"
        "        .word 0\n"
        "        .word 0\n";
    MmMachine *m = mm_create(0);
    int code = -1;
    CHECK(boot(m, pc) == 0);
    CHECK(k_share(m, 0x2000u) == 0);
    CHECK_EQ(cpu_run(m, 8000), 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 7);
    mm_destroy(m);
}

static void test_deadlock(void) {
    static const char *dead =
        "        .text\n"
        "        ld    r0, #8\n"
        "        ld    r1, #1\n"
        "        syscall\n"
        "        mov   r4, r0\n"
        "        ld    r0, #8\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        mov   r5, r0\n"
        "        ld    r0, #3\n"
        "        syscall\n"
        "        cmpi  r0, #0\n"
        "        jne   parent\n"
        "        ld    r0, #9\n"
        "        mov   r1, r4\n"
        "        syscall\n"
        "        jmp   spin\n"
        "parent: ld    r0, #9\n"
        "        mov   r1, r4\n"
        "        syscall\n"
        "        ld    r0, #9\n"
        "        mov   r1, r5\n"
        "        syscall\n"
        "spin:   jmp   spin\n";
    MmMachine *m = mm_create(0);
    int st1 = -1;
    int st2 = -1;
    int value = 0;
    int nwait = 0;
    CHECK(boot(m, dead) == 0);
    CHECK_EQ(cpu_run(m, 2000), 1);
    CHECK(k_exited(m, NULL) == 0);
    CHECK_EQ(k_current(m), 0);
    CHECK(k_pcb(m, 1, &st1, NULL) == 0);
    CHECK(k_pcb(m, 2, &st2, NULL) == 0);
    CHECK_EQ(st1, K_ST_BLOCK);
    CHECK_EQ(st2, K_ST_BLOCK);
    CHECK(k_sem_stat(m, 0, &value, &nwait) == 0);
    CHECK(k_sem_ok(value, nwait));
    CHECK(value < 0);
    CHECK(k_sem_stat(m, 1, &value, &nwait) == 0);
    CHECK(k_sem_ok(value, nwait));
    CHECK(value < 0);
    mm_destroy(m);
}

static int race(const char *body, int quantum) {
    static const char *tail =
        "        cmpi  r3, #0\n"
        "        jne   parent\n"
        "        ld    r0, #0\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "parent: ld    r0, #5\n"
        "        ld    r1, #0\n"
        "        syscall\n"
        "        ld    r1, [r2]\n"
        "        ld    r0, #0\n"
        "        syscall\n"
        "        .org  0x2000\n"
        "cnt:    .word 0\n"
        "lock:   .word 0\n";
    char src[1200];
    MmMachine *m = mm_create(0);
    int code = -1;
    int rc;
    snprintf(src, sizeof src,
             "        .text\n"
             "        la    r2, cnt\n"
             "        la    r6, lock\n"
             "        ld    r4, #4\n"
             "        ld    r0, #3\n"
             "        syscall\n"
             "        mov   r3, r0\n"
             "loop:\n"
             "%s"
             "        subi  r4, r4, #1\n"
             "        jnz   loop\n"
             "%s",
             body, tail);
    rc = boot(m, src);
    if (rc == 0) {
        rc = k_share(m, 0x2000u);
    }
    if (rc == 0) {
        rc = k_proc_policy(m, K_RR, quantum, 1);
    }
    if (rc == 0 && cpu_run(m, 30000) == 0 && k_exited(m, &code)) {
        mm_destroy(m);
        return code;
    }
    mm_destroy(m);
    return -1;
}

static void test_race_lock(void) {
    int open_n = race("        ld    r1, [r2]\n"
                      "        addi  r1, r1, #1\n"
                      "        st    r1, [r2]\n",
                      1);
    int tsl = race("spin:   ld    r1, #1\n"
                   "        tsl   r1, [r6]\n"
                   "        cmpi  r1, #0\n"
                   "        jne   spin\n"
                   "        ld    r1, [r2]\n"
                   "        addi  r1, r1, #1\n"
                   "        st    r1, [r2]\n"
                   "        ld    r1, #0\n"
                   "        st    r1, [r6]\n",
                   1);
    int xchg = race("spin:   ld    r1, #1\n"
                    "        xchg  r1, [r6]\n"
                    "        cmpi  r1, #0\n"
                    "        jne   spin\n"
                    "        ld    r1, [r2]\n"
                    "        addi  r1, r1, #1\n"
                    "        st    r1, [r2]\n"
                    "        ld    r1, #0\n"
                    "        st    r1, [r6]\n",
                    1);
    CHECK(open_n > 0);
    CHECK(open_n < 8);
    CHECK_EQ(tsl, 8);
    CHECK_EQ(xchg, 8);
}

void test_live(void) {
    test_sem_block();
    test_pc();
    test_deadlock();
    test_race_lock();
}
