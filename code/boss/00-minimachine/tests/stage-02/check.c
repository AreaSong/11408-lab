#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_exec.h"
#include "cpu/cpu_intr.h"
#include "cpu/cpu_pipe.h"
#include "kernel/k_trap.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MmMachine *open_pipe(const char *src, const CpuPipeOpt *opt, AsmImage **img) {
    char err[256];
    MmMachine *m;
    *img = NULL;
    if (asm_assemble(src, img, err, sizeof err) != 0) {
        return NULL;
    }
    m = mm_create(0);
    if (m == NULL || mm_load(m, *img) != 0) {
        asm_free(*img);
        mm_destroy(m);
        return NULL;
    }
    cpu_pipe_set(m, opt, 1);
    return m;
}

static void expect_run(const char *src, const CpuPipeOpt *opt, int n, int cycles, int extra) {
    AsmImage *img = NULL;
    MmMachine *m = open_pipe(src, opt, &img);
    CpuPipeStat st;
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    cpu_pipe_run(m, n);
    cpu_pipe_stat(m, &st);
    CHECK_EQ(st.committed, n);
    CHECK_EQ(st.cycles, cycles);
    CHECK_EQ(st.extra, extra);
    asm_free(img);
    mm_destroy(m);
}

static void test_ideal_and_raw(void) {
    CpuPipeOpt opt;
    static const char *indep = "add r1, r2, r3\nadd r4, r5, r6\nadd r0, r2, r3\nadd r7, r5, r6\nhlt\n";
    static const char *raw = "add r1, r2, r3\nsub r4, r1, r5\nand r6, r1, r0\nor r7, r1, r2\nhlt\n";
    AsmImage *img = NULL;
    MmMachine *m;
    cpu_pipe_opt_default(&opt);
    expect_run(indep, &opt, 4, 8, 0);
    opt.forward = 0;
    expect_run(raw, &opt, 4, 10, 2);
    m = open_pipe(raw, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    cpu_pipe_run(m, 4);
    CHECK_EQ(cpu_pipe_id_extra(m, 1), 2);
    CHECK_EQ(cpu_pipe_id_extra(m, 2), 0);
    CHECK_EQ(cpu_pipe_id_extra(m, 3), 0);
    asm_free(img);
    mm_destroy(m);
    opt.forward = 1;
    expect_run(raw, &opt, 4, 8, 0);
}

static void test_load_use_and_2019(void) {
    CpuPipeOpt opt;
    static const char *use = "ld r1, [r2]\nadd r3, r1, r4\nhlt\n";
    static const char *y2019 = "add r2, r1, r0\nld r3, [r4]\nadd r2, r2, r3\nst r2, [r4]\nhlt\n";
    AsmImage *img = NULL;
    MmMachine *m;
    cpu_pipe_opt_default(&opt);
    m = open_pipe(use, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    mm_set_reg(m, 2, 0x2000);
    mm_write(m, 0x2000, 4, 9);
    cpu_pipe_run(m, 2);
    CHECK_EQ(mm_cycles(m), 7);
    CHECK_EQ(cpu_pipe_id_extra(m, 1), 1);
    CHECK_EQ(mm_reg(m, 1), 9);
    asm_free(img);
    mm_destroy(m);
    m = open_pipe(y2019, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    mm_set_reg(m, 0, 1);
    mm_set_reg(m, 1, 2);
    mm_set_reg(m, 4, 0x2000);
    mm_write(m, 0x2000, 4, 10);
    cpu_pipe_run(m, 4);
    CHECK_EQ(cpu_pipe_id_extra(m, 1), 0);
    CHECK_EQ(cpu_pipe_id_extra(m, 2), 1);
    CHECK_EQ(cpu_pipe_id_extra(m, 3), 0);
    {
        CpuPipeStat st;
        cpu_pipe_stat(m, &st);
        CHECK_EQ(st.cycles, 9);
        CHECK_EQ(st.extra, 1);
    }
    asm_free(img);
    mm_destroy(m);
}

static void test_2016(void) {
    CpuPipeOpt opt;
    static const char *src = "add r1, r2, r3\nadd r5, r2, r4\nadd r4, r5, r3\nadd r5, r2, r6\nhlt\n";
    AsmImage *img = NULL;
    MmMachine *m;
    cpu_pipe_opt_default(&opt);
    opt.forward = 0;
    m = open_pipe(src, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    cpu_pipe_run(m, 4);
    CHECK_EQ(cpu_pipe_id_extra(m, 1), 0);
    CHECK_EQ(cpu_pipe_id_extra(m, 2), 2);
    CHECK_EQ(cpu_pipe_id_extra(m, 3), 0);
    asm_free(img);
    mm_destroy(m);
}

static void test_2023(void) {
    static const char *src =
        "add r2, r1, r0\nld r3, [r2]\njr r3\ndest: addi r4, r4, #20\nhlt\n.data\nslot: .word dest\n";
    CpuPipeOpt opt;
    AsmImage *img = NULL;
    MmMachine *m;
    uint32_t slot = 0;
    CpuPipeStat st;
    cpu_pipe_opt_default(&opt);
    opt.branch_stall = 1;
    m = open_pipe(src, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    CHECK(asm_symbol(img, "slot", &slot) == 0);
    mm_set_reg(m, 0, slot);
    cpu_pipe_run(m, 4);
    cpu_pipe_stat(m, &st);
    CHECK_EQ(st.cycles, 12);
    CHECK_EQ(st.extra, 4);
    CHECK_EQ(cpu_pipe_id_extra(m, 1), 0);
    CHECK_EQ(cpu_pipe_id_extra(m, 2), 1);
    CHECK_EQ(cpu_pipe_ctrl_of(m, 3), 3);
    CHECK_EQ(mm_reg(m, 4), 20);
    asm_free(img);
    mm_destroy(m);
}

static void test_2014(void) {
    static const char *src = "shl r4, r2, #2\nadd r4, r4, r3\nld r5, [r4]\nadd r1, r1, r5\n"
                             "addi r2, r2, #1\njr r2\nloop: shl r4, r2, #2\nhlt\n";
    CpuPipeOpt opt;
    AsmImage *img = NULL;
    MmMachine *m;
    uint32_t loop = 0;
    uint32_t base;
    CpuPipeStat st;
    cpu_pipe_opt_default(&opt);
    opt.forward = 0;
    opt.branch_stall = 1;
    m = open_pipe(src, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    CHECK(asm_symbol(img, "loop", &loop) == 0);
    base = loop - 1u;
    mm_set_reg(m, 2, base);
    mm_set_reg(m, 3, 0x2000u - (base << 2));
    mm_write(m, 0x2000, 4, 3);
    cpu_pipe_run(m, 7);
    cpu_pipe_stat(m, &st);
    CHECK_EQ(st.cycles, 22);
    CHECK_EQ(st.extra, 11);
    CHECK_EQ(cpu_pipe_id_extra(m, 1), 2);
    CHECK_EQ(cpu_pipe_id_extra(m, 2), 2);
    CHECK_EQ(cpu_pipe_id_extra(m, 3), 2);
    CHECK_EQ(cpu_pipe_id_extra(m, 4), 0);
    CHECK_EQ(cpu_pipe_id_extra(m, 5), 2);
    CHECK_EQ(cpu_pipe_id_extra(m, 6), 0);
    CHECK_EQ(cpu_pipe_ctrl_of(m, 6), 3);
    asm_free(img);
    mm_destroy(m);
}

static void test_struct_and_branch(void) {
    CpuPipeOpt opt;
    static const char *uni = "ld r1, [r2]\nadd r3, r4, r5\nadd r6, r0, r7\nadd r0, r4, r5\nhlt\n";
    static const char *taken = "jmp dest\nadd r1, r2, r3\nadd r4, r5, r6\ndest: nop\nhlt\n";
    static const char *fall = "jne dest\nadd r3, r4, r5\nadd r6, r0, r1\ndest: nop\nhlt\n";
    AsmImage *img = NULL;
    MmMachine *m;
    CpuPipeStat st;
    cpu_pipe_opt_default(&opt);
    opt.unified = 1;
    m = open_pipe(uni, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    mm_set_reg(m, 2, 0x2000);
    cpu_pipe_run(m, 4);
    cpu_pipe_stat(m, &st);
    CHECK_EQ(st.cycles, 9);
    CHECK_EQ(st.extra, 1);
    CHECK_EQ(st.struct_stall, 1);
    asm_free(img);
    mm_destroy(m);
    cpu_pipe_opt_default(&opt);
    m = open_pipe(taken, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    cpu_pipe_run(m, 2);
    cpu_pipe_stat(m, &st);
    CHECK_EQ(st.cycles, 8);
    CHECK_EQ(st.extra, 2);
    CHECK_EQ(st.flushed, 2);
    CHECK_EQ(st.committed, 2);
    asm_free(img);
    mm_destroy(m);
    m = open_pipe(fall, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    mm_set_psw(m, MM_PSW_ZF);
    cpu_pipe_run(m, 3);
    cpu_pipe_stat(m, &st);
    CHECK_EQ(st.cycles, 7);
    CHECK_EQ(st.extra, 0);
    CHECK_EQ(st.flushed, 0);
    asm_free(img);
    mm_destroy(m);
}

static void test_precise(void) {
    static const char *div = "div r1, r2, r3\nst r4, [r5]\nadd r6, r6, r6\nhlt\n";
    static const char *page = "ld r1, [r2]\nadd r3, r1, r4\nhlt\n";
    CpuPipeOpt opt;
    AsmImage *img = NULL;
    MmMachine *m;
    uint32_t word = 0;
    cpu_pipe_opt_default(&opt);
    m = open_pipe(div, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    mm_set_reg(m, 2, 1);
    mm_set_reg(m, 4, 0x11111111u);
    mm_set_reg(m, 5, 0x2000);
    mm_set_reg(m, 6, 5);
    mm_write(m, 0x2000, 4, 0x22222222u);
    CHECK_EQ(cpu_pipe_run(m, 8), 0);
    CHECK_EQ(mm_cause(m), MM_EX_DIV);
    CHECK_EQ(mm_reg(m, 6), 5);
    CHECK_EQ(mm_reg(m, 1), 0);
    mm_read(m, 0x2000, 4, &word);
    CHECK_EQ(word, 0x22222222u);
    asm_free(img);
    mm_destroy(m);
    m = open_pipe(page, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    mm_set_psw(m, MM_PSW_PG);
    mm_set_reg(m, 2, 0x2000);
    mm_set_reg(m, 3, 7);
    CHECK_EQ(cpu_pipe_run(m, 8), 0);
    CHECK_EQ(mm_cause(m), MM_EX_PAGE);
    CHECK_EQ(mm_badva(m), 0x2000u);
    CHECK_EQ(mm_reg(m, 1), 0);
    CHECK_EQ(mm_reg(m, 3), 7);
    asm_free(img);
    mm_destroy(m);
}

static int run_mode(const char *src, int pipe, uint32_t regs[8], uint32_t *pc, uint32_t *word) {
    char err[256];
    AsmImage *img = NULL;
    MmMachine *m;
    CpuPipeOpt opt;
    uint32_t arr = 0;
    int i;
    if (asm_assemble(src, &img, err, sizeof err) != 0) {
        return -1;
    }
    m = mm_create(0);
    cpu_pipe_opt_default(&opt);
    cpu_pipe_set(m, &opt, pipe);
    if (asm_symbol(img, "arr", &arr) != 0 || mm_load(m, img) != 0 || cpu_run(m, 10000) != 0 ||
        mm_read(m, arr, 4, word) != 0) {
        asm_free(img);
        mm_destroy(m);
        return -1;
    }
    for (i = 0; i < 8; i++) {
        regs[i] = mm_reg(m, i);
    }
    *pc = mm_pc(m);
    asm_free(img);
    mm_destroy(m);
    return 0;
}

static void test_same_state(void) {
    static const char *src =
        "ld r0, #0\nla r2, arr\nld r1, #4\nloop: ld r3, [r2]\nadd r0, r0, r3\naddi r2, r2, #4\n"
        "subi r1, r1, #1\njnz loop\nhlt\n.data\narr: .word 1, 2, 3, 4\n";
    uint32_t a[8], b[8], pa = 0, pb = 0, wa = 0, wb = 1;
    int i;
    CHECK(run_mode(src, 0, a, &pa, &wa) == 0);
    CHECK(run_mode(src, 1, b, &pb, &wb) == 0);
    CHECK_EQ(pa, pb);
    CHECK_EQ(wa, wb);
    for (i = 0; i < 8; i++) {
        CHECK_EQ(a[i], b[i]);
    }
    CHECK_EQ(b[0], 10);
}

static void test_syscall(void) {
    static const char *src =
        "ld r0, #1\nld r1, #1\nla r2, msg\nld r3, #5\nsyscall\nspin: jmp spin\n.data\nmsg: .asciz \"hello\"\n";
    CpuPipeOpt opt;
    AsmImage *img = NULL;
    MmMachine *m;
    char buf[16];
    char *text = NULL;
    size_t len = 0;
    FILE *event;
    cpu_pipe_opt_default(&opt);
    m = open_pipe(src, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    CHECK(k_boot(m) == 0);
    mm_set_psw(m, MM_PSW_U);
    event = open_memstream(&text, &len);
    cpu_set_event(m, event);
    CHECK_EQ(cpu_pipe_run(m, 40), 1);
    fclose(event);
    CHECK_EQ(mm_reg(m, 0), 5);
    CHECK(k_console_out(m, buf, (int)sizeof buf) == 5);
    CHECK_STR(buf, "hello");
    CHECK(strstr(text, "系统调用顺序 3-2-4-1") != NULL);
    free(text);
    asm_free(img);
    mm_destroy(m);
}

static void test_metrics(void) {
    static const char *src = "add r1, r2, r3\nadd r4, r5, r6\nadd r0, r2, r3\nadd r7, r5, r6\nhlt\n";
    CpuPipeOpt opt;
    AsmImage *img = NULL;
    MmMachine *m;
    char *text = NULL;
    size_t len = 0;
    FILE *fp;
    cpu_pipe_opt_default(&opt);
    opt.diagram = 1;
    opt.freq_mhz = 100;
    m = open_pipe(src, &opt, &img);
    CHECK(m != NULL);
    if (m == NULL) {
        return;
    }
    cpu_pipe_run(m, 4);
    fp = open_memstream(&text, &len);
    cpu_pipe_report(m, fp);
    fclose(fp);
    CHECK(strstr(text, "理想 8 个时钟周期，实际 8 个时钟周期") != NULL);
    CHECK(strstr(text, "结果：阻塞 0 拍，共 8 个时钟周期") != NULL);
    CHECK(strstr(text, "执行时间 = 8 × 10 ns = 80 ns") != NULL);
    CHECK(strstr(text, "MIPS = 100 / (8/4) = 50") != NULL);
    CHECK(strstr(text, "效率 E = (4×5)/(5×8) = 1/2") != NULL);
    free(text);
    asm_free(img);
    mm_destroy(m);
}

int main(void) {
    test_ideal_and_raw();
    test_load_use_and_2019();
    test_2016();
    test_2023();
    test_2014();
    test_struct_and_branch();
    test_precise();
    test_same_state();
    test_syscall();
    test_metrics();
    return TEST_SUMMARY();
}
