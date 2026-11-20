#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_decode.h"
#include "cpu/cpu_exec.h"
#include "cpu/cpu_intr.h"
#include "kernel/k_trap.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_class(void) {
    CpuEventInfo info;
    CHECK(cpu_event_info(MM_EX_DIV, &info) == 0);
    CHECK(strcmp(info.klass, "故障") == 0);
    CHECK(strcmp(info.repair, "不能修复") == 0);
    CHECK(strcmp(info.epc, "本条") == 0);
    CHECK(!info.maskable);
    CHECK(cpu_event_info(MM_EX_PAGE, &info) == 0);
    CHECK(strcmp(info.repair, "能修复") == 0);
    CHECK(cpu_event_info(MM_EX_SYSCALL, &info) == 0);
    CHECK(strcmp(info.klass, "陷阱") == 0);
    CHECK(strcmp(info.epc, "下一条") == 0);
    CHECK(cpu_event_info(MM_EX_MCHK, &info) == 0);
    CHECK(strcmp(info.klass, "终止") == 0);
    CHECK(strcmp(info.epc, "无") == 0);
    CHECK(cpu_event_info(MM_EX_NMI, &info) == 0);
    CHECK(strcmp(info.klass, "外中断") == 0);
    CHECK(!info.maskable);
    CHECK(strcmp(info.when, "指令结束") == 0);
    CHECK(cpu_event_info(16, &info) == 0);
    CHECK(info.maskable);
    CHECK(strcmp(info.epc, "下一条") == 0);
    CHECK(cpu_is_privileged(MM_OP_HLT));
    CHECK(cpu_is_privileged(MM_OP_DI));
    CHECK(cpu_is_privileged(MM_OP_OUT));
    CHECK(!cpu_is_privileged(MM_OP_SYSCALL));
    CHECK(!cpu_is_privileged(MM_OP_R));
}

static void test_legacy_fault(void) {
    MmMachine *m = mm_create(0);
    mm_write(m, 0x1000, 4, cpu_enc_r(0, 1, 2, 0, MM_FN_DIV));
    mm_set_reg(m, 1, 0x80000000u);
    mm_set_reg(m, 2, (uint32_t)-1);
    mm_set_pc(m, 0x1000);
    cpu_step(m);
    CHECK_EQ(mm_cause(m), MM_EX_DIV);
    CHECK_EQ(mm_pc(m), 0x1000u);
    CHECK_EQ(mm_reg(m, 0), 0);
    CHECK_EQ(mm_sr(m, MM_SR_EPC), 0);
    mm_destroy(m);
}

static void put_nop(MmMachine *m) {
    mm_write(m, 0x1000, 4, cpu_enc_r(0, 0, 0, 0, MM_FN_NOP));
    mm_set_pc(m, 0x1000);
}

static void test_fault_enters_kernel(void) {
    MmMachine *m = mm_create(0);
    CHECK(k_boot(m) == 0);
    mm_write(m, 0x1000, 4, cpu_enc_i(MM_OP_HLT, 0, 0, 0));
    mm_set_pc(m, 0x1000);
    mm_set_psw(m, MM_PSW_U | MM_PSW_IF);
    cpu_step(m);
    CHECK(mm_halted(m));
    CHECK_EQ(mm_cause(m), MM_EX_PRIV);
    CHECK_EQ(mm_sr(m, MM_SR_EPC), 0x1000u);
    CHECK_EQ(mm_pc(m), cpu_host_entry(MM_EX_PRIV));
    CHECK((mm_psw(m) & MM_PSW_U) == 0);
    CHECK((mm_psw(m) & MM_PSW_IF) == 0);
    mm_destroy(m);
}

static int run_src(const char *src, MmMachine **out, FILE *event) {
    AsmImage *img = NULL;
    char err[160];
    MmMachine *m;
    if (asm_assemble(src, &img, err, sizeof err) != 0) {
        return -1;
    }
    m = mm_create(0);
    if (mm_load(m, img) != 0 || k_boot(m) != 0) {
        asm_free(img);
        mm_destroy(m);
        return -1;
    }
    asm_free(img);
    mm_set_psw(m, MM_PSW_U);
    cpu_set_event(m, event);
    *out = m;
    return cpu_run(m, 40);
}

static void test_syscall_write(void) {
    static const char src[] =
        "ld r0, #1\nld r1, #1\nla r2, msg\nld r3, #5\nsyscall\nspin: jmp spin\n.data\nmsg: .asciz \"hello\"\n";
    char *text = NULL;
    size_t len = 0;
    FILE *event = open_memstream(&text, &len);
    MmMachine *m = NULL;
    char buf[32];
    CHECK(event != NULL);
    CHECK(run_src(src, &m, event) == 1);
    fclose(event);
    CHECK_EQ(mm_reg(m, 0), 5);
    CHECK((mm_psw(m) & MM_PSW_U) != 0);
    CHECK(k_console_out(m, buf, (int)sizeof buf) == 5);
    CHECK(strcmp(buf, "hello") == 0);
    CHECK(strstr(text, "传参 执行者=用户程序 顺序=3") != NULL);
    CHECK(strstr(text, "SYSCALL 执行者=硬件 顺序=2") != NULL);
    CHECK(strstr(text, "现场 执行者=操作系统 顺序=4 保存=R0-R7") != NULL);
    CHECK(strstr(text, "服务 执行者=操作系统 顺序=4 调用=write") != NULL);
    CHECK(strstr(text, "IRET 执行者=硬件 安排=操作系统 顺序=1") != NULL);
    CHECK(strstr(text, "系统调用顺序 3-2-4-1") != NULL);
    CHECK(strstr(text, "模式 用户态->内核态 执行者=硬件 原因=系统调用") != NULL);
    CHECK(strstr(text, "模式 内核态->用户态 执行者=硬件 经由=IRET") != NULL);
    free(text);
    mm_destroy(m);
}

static void test_exit_and_read(void) {
    static const char src[] =
        "ld r0, #2\nld r1, #0\nla r2, buf\nld r3, #8\nsyscall\n"
        "ld r0, #0\nld r1, #7\nsyscall\n.data\nbuf: .space 8\n";
    MmMachine *m = NULL;
    char buf[16];
    uint32_t byte = 0;
    uint32_t addr = 0;
    int code = 0;
    AsmImage *img = NULL;
    char err[160];
    CHECK(asm_assemble(src, &img, err, sizeof err) == 0);
    CHECK(asm_symbol(img, "buf", &addr) == 0);
    m = mm_create(0);
    CHECK(mm_load(m, img) == 0);
    asm_free(img);
    CHECK(k_boot(m) == 0);
    k_console_set_in(m, "ab");
    mm_set_psw(m, MM_PSW_U);
    CHECK(cpu_run(m, 40) == 0);
    CHECK(k_exited(m, &code));
    CHECK_EQ(code, 7);
    CHECK(k_console_out(m, buf, (int)sizeof buf) == 0);
    CHECK(mm_read(m, addr, 1, &byte) == 0);
    CHECK_EQ(byte, (uint32_t)'a');
    CHECK(mm_read(m, addr + 1, 1, &byte) == 0);
    CHECK_EQ(byte, (uint32_t)'b');
    mm_destroy(m);
}

static void test_nest_order(void) {
    MmMachine *m = mm_create(0);
    int order[8];
    int n;
    CHECK(k_boot(m) == 0);
    k_set_irq_mask(m, 0, 0x7);
    k_set_irq_mask(m, 1, 0x2);
    k_set_irq_mask(m, 2, 0x6);
    k_set_irq_mask(m, 3, 0xF);
    cpu_raise_irq(m, 0);
    cpu_raise_irq(m, 1);
    cpu_raise_irq(m, 2);
    cpu_raise_irq(m, 3);
    mm_set_psw(m, MM_PSW_IF);
    put_nop(m);
    cpu_step(m);
    n = k_last_order(m, order, 8);
    CHECK_EQ(n, 4);
    CHECK_EQ(order[0], 3);
    CHECK_EQ(order[1], 0);
    CHECK_EQ(order[2], 2);
    CHECK_EQ(order[3], 1);
    CHECK_EQ(mm_pc(m), 0x1004u);
    CHECK((mm_psw(m) & MM_PSW_IF) != 0);
    mm_destroy(m);
}

static void test_priority_and_mask(void) {
    MmMachine *m = mm_create(0);
    int order[4];
    CHECK(k_boot(m) == 0);
    cpu_raise_irq(m, 3);
    cpu_raise_irq(m, 0);
    mm_set_psw(m, MM_PSW_IF);
    put_nop(m);
    cpu_step(m);
    CHECK_EQ(k_last_order(m, order, 4), 2);
    CHECK_EQ(order[0], 0);
    CHECK_EQ(order[1], 3);
    mm_reset(m);
    CHECK(k_boot(m) == 0);
    cpu_raise_irq(m, 3);
    put_nop(m);
    cpu_step(m);
    CHECK(cpu_irq_pending(m, 3));
    CHECK_EQ(k_last_order(m, order, 4), 0);
    mm_set_psw(m, MM_PSW_IF);
    cpu_set_imr(m, 1u << 3);
    cpu_step(m);
    CHECK(cpu_irq_pending(m, 3));
    cpu_raise_nmi(m);
    cpu_step(m);
    CHECK_EQ(mm_sr(m, MM_SR_CAUSE), (uint32_t)MM_EX_NMI);
    CHECK(cpu_irq_pending(m, 3));
    mm_destroy(m);
}

static void test_timer_break_trap(void) {
    MmMachine *m = mm_create(0);
    int order[4];
    uint32_t word;
    CHECK(k_boot(m) == 0);
    cpu_timer_set(m, 5);
    mm_set_psw(m, MM_PSW_IF);
    put_nop(m);
    cpu_step(m);
    CHECK_EQ(k_last_order(m, order, 4), 1);
    CHECK_EQ(order[0], 0);
    mm_reset(m);
    CHECK(k_boot(m) == 0);
    word = cpu_enc_i(MM_OP_BRK, 0, 0, 0);
    mm_write(m, 0x1000, 4, word);
    mm_set_pc(m, 0x1000);
    cpu_step(m);
    CHECK(!mm_halted(m));
    CHECK_EQ(mm_pc(m), 0x1004u);
    CHECK_EQ(mm_sr(m, MM_SR_EPC), 0x1004u);
    CHECK_EQ(mm_sr(m, MM_SR_CAUSE), (uint32_t)MM_EX_BREAK);
    mm_destroy(m);
}

static void test_align(void) {
    MmMachine *m = mm_create(0);
    CHECK(k_boot(m) == 0);
    mm_write(m, 0x1000, 4, cpu_enc_m(MM_OP_LD, 0, 1, MM_MODE_REG, 0));
    mm_set_reg(m, 1, 1);
    mm_set_pc(m, 0x1000);
    mm_set_psw(m, MM_PSW_U);
    cpu_step(m);
    CHECK(mm_halted(m));
    CHECK_EQ(mm_cause(m), MM_EX_ALIGN);
    CHECK_EQ(mm_sr(m, MM_SR_EPC), 0x1000u);
    CHECK_EQ(mm_badva(m), 1);
    CHECK_EQ(mm_sr(m, MM_SR_BADVA), 1);
    mm_destroy(m);
}

int main(void) {
    test_class();
    test_legacy_fault();
    test_fault_enters_kernel();
    test_syscall_write();
    test_exit_and_read();
    test_nest_order();
    test_priority_and_mask();
    test_timer_break_trap();
    test_align();
    return TEST_SUMMARY();
}
