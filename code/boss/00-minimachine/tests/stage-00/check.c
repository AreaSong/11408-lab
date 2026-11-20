#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_alu.h"
#include "cpu/cpu_decode.h"
#include "cpu/cpu_exec.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void roundtrip(uint32_t word, uint32_t pc) {
    char text[80];
    char err[80];
    uint32_t back[2];
    int n = 0;
    CHECK(cpu_disasm(word, pc, text, sizeof text) == 0);
    CHECK(asm_encode_line(text, pc, back, &n, err, sizeof err) == 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(back[0], word);
}

static void run_src(const char *src, MmMachine **out) {
    char err[160];
    AsmImage *img = NULL;
    MmMachine *m;
    CHECK(asm_assemble(src, &img, err, sizeof err) == 0);
    m = mm_create(0);
    CHECK(m != NULL);
    CHECK(mm_load(m, img) == 0);
    CHECK_EQ(cpu_run(m, 10000), 0);
    asm_free(img);
    *out = m;
}

static void test_encode(void) {
    char text[80];
    CHECK_EQ(cpu_enc_r(1, 2, 3, 0, MM_FN_ADD), 0x00a60001u);
    CHECK_EQ(cpu_enc_m(MM_OP_LD, 1, 0, MM_MODE_IMM, 10), 0x4080000au);
    CHECK_EQ(cpu_enc_b(MM_OP_JCC, MM_COND_JE, 8), 0x80400008u);
    CHECK_EQ(cpu_enc_i(MM_OP_HLT, 0, 0, 0), 0xd0000000u);
    roundtrip(0x00a60001u, 0x1000);
    roundtrip(cpu_enc_m(MM_OP_LD, 1, 2, MM_MODE_BASE, (uint16_t)-8), 0x1000);
    roundtrip(cpu_enc_m(MM_OP_LD, 1, 3, MM_MODE_IDX, 0x2000), 0x2000);
    roundtrip(cpu_enc_b(MM_OP_JCC, MM_COND_JNE, (uint16_t)-20), 0x1020);
    CHECK(cpu_disasm(0, 0x1000, text, sizeof text) == 0);
    CHECK_STR(text, "nop");
    {
        CpuInsn bad;
        CHECK(cpu_decode(0x00a60001u | (1u << 11), &bad) != 0);
    }
}

static void test_flags(void) {
    CpuAlu add = cpu_alu_add(0x87654321u, 0x98765432u);
    CpuAlu cmp = cpu_alu_sub(0, 0xffffffffu);
    CpuAlu sub = cpu_alu_sub(10, (uint32_t)-20);
    CpuAlu sh = cpu_alu_shift(0x80000000u, 1, CPU_SHL, 0);
    CpuAlu sar = cpu_alu_shift(0x80000000u, 1, CPU_SAR, 1);
    CpuAlu hold = cpu_alu_shift(0x1, 0, CPU_SHL, 1);
    CpuAlu mul = cpu_alu_mul(0x40000000u, 2);
    uint32_t quot = 1;
    CHECK_EQ(add.result, 0x1fdb9753u);
    CHECK_EQ(add.cf, 1);
    CHECK_EQ(add.of, 1);
    CHECK_EQ(cmp.result, 1);
    CHECK_EQ(cmp.cf, 1);
    CHECK_EQ(cmp.of, 0);
    CHECK_EQ(sub.result, 30);
    CHECK_EQ(sub.of, 0);
    CHECK_EQ(sub.cf, 1);
    CHECK_EQ(sh.result, 0);
    CHECK_EQ(sh.cf, 1);
    CHECK_EQ(sar.result, 0xc0000000u);
    CHECK_EQ(hold.cf, 1);
    CHECK_EQ(mul.of, 1);
    CHECK_EQ(mul.cf, 1);
    CHECK_EQ(mul.result, 0x80000000u);
    CHECK(cpu_alu_div(0x80000000u, (uint32_t)-1, 0, &quot) != 0);
    CHECK_EQ(cpu_cond(MM_COND_JA, 0), 1);
    CHECK_EQ(cpu_cond(MM_COND_JA, MM_PSW_CF), 0);
    CHECK_EQ(cpu_cond(MM_COND_JL, MM_PSW_SF), 1);
    CHECK_EQ(cpu_cond(MM_COND_JL, MM_PSW_SF | MM_PSW_OF), 0);
}

static void test_ea(void) {
    CpuEa ea;
    uint32_t base = cpu_enc_m(MM_OP_LD, 1, 2, MM_MODE_BASE, 8);
    uint32_t ind = cpu_enc_m(MM_OP_LD, 1, 0, MM_MODE_IND, 0x2000);
    uint32_t rel = cpu_enc_m(MM_OP_LD, 1, 0, MM_MODE_REL, 12);
    uint32_t lea = cpu_enc_m(MM_OP_LEA, 1, 0, MM_MODE_DIR, 0x2000);
    CHECK(cpu_ea(base, 0x1000, 0x2000, 0, &ea) == 0);
    CHECK_EQ(ea.ea, 0x2008u);
    CHECK_EQ(ea.mem_ops, 1);
    CHECK(cpu_ea(ind, 0x1000, 0, 0x3000, &ea) == 0);
    CHECK_EQ(ea.ea, 0x3000u);
    CHECK_EQ(ea.mem_ops, 2);
    CHECK(cpu_ea(rel, 0x1000, 0, 0, &ea) == 0);
    CHECK_EQ(ea.ea, 0x1010u);
    CHECK(cpu_ea(lea, 0x1000, 0, 0, &ea) == 0);
    CHECK_EQ(ea.mem_ops, 0);
    CHECK_EQ(cpu_ea(cpu_enc_m(MM_OP_LD, 1, 0, 0, 5), 0x1000, 0, 0, &ea), 0);
    CHECK_EQ(ea.has_ea, 0);
    CHECK_EQ(ea.mem_ops, 0);
}

static void test_bus_and_add(void) {
    MmMachine *m = mm_create(0);
    FILE *fp = tmpfile();
    char buf[2000];
    size_t n;
    CHECK_EQ(cpu_bus_drivers("PCout MARin IPCin"), 1);
    CHECK_EQ(cpu_bus_drivers("MemR MDRinE PC+4"), 0);
    CHECK_EQ(cpu_bus_drivers("MDRoutE MemW"), 0);
    CHECK_EQ(cpu_bus_drivers("RSout RTout"), 2);
    mm_set_reg(m, 1, 0x87654321u);
    mm_set_reg(m, 2, 0x98765432u);
    mm_write(m, 0x1000, 4, cpu_enc_r(0, 1, 2, 0, MM_FN_ADD));
    mm_set_pc(m, 0x1000);
    mm_set_trace(m, fp);
    cpu_step(m);
    CHECK_EQ(mm_reg(m, 0), 0x1fdb9753u);
    CHECK_EQ(mm_cycles(m), 7);
    CHECK_EQ(mm_bus_conflicts(m), 0);
    CHECK((mm_psw(m) & MM_PSW_CF) != 0);
    CHECK((mm_psw(m) & MM_PSW_OF) != 0);
    rewind(fp);
    n = fread(buf, 1, sizeof buf - 1, fp);
    buf[n] = '\0';
    CHECK(strstr(buf, "Y+r2->Z 更新PSW") != NULL);
    CHECK(strstr(buf, "内总线驱动 2 个") == NULL);
    fclose(fp);
    mm_destroy(m);
}

static void test_programs(void) {
    static const char sum[] =
        ".text\n"
        "ld r0, #0\nla r2, arr\nld r1, #10\n"
        "loop: ld r3, [r2]\nadd r0, r0, r3\naddi r2, r2, #4\nsubi r1, r1, #1\njnz loop\nhlt\n"
        ".data\narr: .word 1, 2, 3, 4, 5, 6, 7, 8, 9, 10\n";
    static const char call[] =
        "li sp, #0x7000\nli r1, #6\nli r2, #7\ncall muladd\nhlt\n"
        "muladd: push lr\npush fp\nmov fp, sp\nadd r0, r1, r2\n"
        "mov sp, fp\npop fp\npop lr\nret\n";
    MmMachine *m = NULL;
    char err[120];
    AsmImage *img = NULL;
    uint32_t loop = 0;
    uint32_t word = 0;
    run_src(sum, &m);
    CHECK_EQ(mm_reg(m, 0), 55);
    CHECK(mm_halted(m));
    CHECK_EQ(mm_cause(m), -1);
    mm_destroy(m);
    CHECK(asm_assemble("loop: nop\njnz loop\n", &img, err, sizeof err) == 0);
    CHECK(asm_symbol(img, "loop", &loop) == 0);
    m = mm_create(0);
    CHECK(mm_load(m, img) == 0);
    CHECK(mm_read(m, 0x1004, 4, &word) == 0);
    CHECK_EQ((int16_t)(word & 0xffffu), (int)(loop - (0x1004u + 4u)));
    asm_free(img);
    mm_destroy(m);
    run_src(call, &m);
    CHECK_EQ(mm_reg(m, 0), 13);
    CHECK_EQ(mm_reg(m, 7), 0x7000u);
    CHECK_EQ(mm_reg(m, 5), 0);
    mm_destroy(m);
}

static void test_faults(void) {
    MmMachine *m = mm_create(0);
    mm_write(m, 0x1000, 4, cpu_enc_r(0, 1, 2, 0, MM_FN_DIV));
    mm_set_reg(m, 1, 0x80000000u);
    mm_set_reg(m, 2, (uint32_t)-1);
    mm_set_pc(m, 0x1000);
    cpu_step(m);
    CHECK_EQ(mm_cause(m), MM_EX_DIV);
    CHECK_EQ(mm_pc(m), 0x1000u);
    CHECK_EQ(mm_reg(m, 0), 0);
    mm_reset(m);
    mm_write(m, 0x1000, 4, cpu_enc_i(MM_OP_HLT, 0, 0, 0));
    mm_set_psw(m, MM_PSW_U);
    mm_set_pc(m, 0x1000);
    cpu_step(m);
    CHECK_EQ(mm_cause(m), MM_EX_PRIV);
    CHECK_EQ(mm_pc(m), 0x1000u);
    mm_destroy(m);
}

int main(void) {
    test_encode();
    test_flags();
    test_ea();
    test_bus_and_add();
    test_programs();
    test_faults();
    return TEST_SUMMARY();
}
