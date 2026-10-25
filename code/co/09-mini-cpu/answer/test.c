#include "asm.h"
#include "beat.h"
#include "exam.h"
#include "psw.h"
#include "run.h"
#include "test.h"

#include <string.h>

static void test_bus(void) {
    CHECK_EQ(driver_count("PCout MARin"), 1);
    CHECK_EQ(driver_count("MemR MDRinE PC+1"), 0);
    CHECK_EQ(driver_count("MemR MDRinE R0out Ain"), 1);
    CHECK_EQ(driver_count("MDRoutE MemW"), 0);
    CHECK_EQ(driver_count("R1out R0out MARin"), 2);
    CHECK_EQ(driver_count("无"), 0);
}

static void test_flags(void) {
    uint16_t sum;
    Psw psw;
    psw_add(7, 5, &sum, &psw);
    CHECK_EQ(sum, 12);
    CHECK_EQ(psw.zf, 0);
    CHECK_EQ(psw.of, 0);
    psw_add(0x7FFF, 1, &sum, &psw);
    CHECK_EQ(sum, 0x8000);
    CHECK_EQ(psw.sf, 1);
    CHECK_EQ(psw.cf, 0);
    CHECK_EQ(psw.of, 1);
    CHECK_EQ(of_add_bits(0, 0, 1), 1);
    CHECK_EQ(sub_bits(0x8000, 1, &psw), 0x7FFF);
    CHECK_EQ(psw.of, 1);
    CHECK_EQ(psw.sf, 0);
    CHECK_EQ(of_sub_bits(1, 0, 0), 1);
    CHECK_EQ(of_add_bits(1, 0, 0), 0);
    psw_add(1, 0xFFFF, &sum, &psw);
    CHECK_EQ(sum, 0);
    CHECK_EQ(psw.zf, 1);
    CHECK_EQ(psw.cf, 1);
    CHECK_EQ(psw.of, 0);
}

static void test_encode(void) {
    uint16_t word = 0;
    char text[32];
    CHECK(asm_encode("lda 8", &word) == 0);
    CHECK_EQ(word, 0x1008);
    CHECK(asm_encode("adm 1", &word) == 0);
    CHECK_EQ(word, 0x6100);
    CHECK(asm_encode("hlt", &word) == 0);
    CHECK_EQ(word, 0x7000);
    fmt_inst(text, sizeof text, 0x6100);
    CHECK_STR(text, "ADM R1");
    CHECK_EQ(encode15(1, 1, 0, 0, 0, 0), 0x0240);
    CHECK_EQ(encode15(2, 2, 0, 1, 0, 0), 0x0488);
    CHECK_EQ(encode15(3, 3, 1, 1, 0, 2), 0x06EA);
}

static void run_words(const uint16_t *code, int ncode, Cpu *cpu) {
    Program prog;
    FILE *fp = tmpfile();
    memset(&prog, 0, sizeof prog);
    memcpy(prog.code, code, (size_t)ncode * sizeof code[0]);
    prog.ncode = ncode;
    CHECK(fp != NULL);
    CHECK(cpu_exec(&prog, cpu, fp) == 0);
    if (fp != NULL) {
        fclose(fp);
    }
}

static void test_add(void) {
    Cpu cpu;
    Program prog;
    FILE *fp = tmpfile();
    uint16_t add[] = {0x1008, 0x3009, 0x200A, 0x7000};
    memset(&prog, 0, sizeof prog);
    memcpy(prog.code, add, sizeof add);
    prog.ncode = 4;
    prog.daddr[0] = 8;
    prog.dval[0] = 7;
    prog.daddr[1] = 9;
    prog.dval[1] = 5;
    prog.ndata = 2;
    CHECK(fp != NULL);
    CHECK(cpu_exec(&prog, &cpu, fp) == 0);
    if (fp != NULL) {
        fclose(fp);
    }
    CHECK_EQ(cpu.acc, 12);
    CHECK_EQ(cpu.mem[10], 12);
    CHECK_EQ(cpu.visits, 7);
    CHECK_EQ(cpu.conflicts, 0);
    CHECK_EQ(cpu.psw.zf, 0);
}

static void test_jz_adm(void) {
    Program prog;
    Cpu cpu;
    FILE *fp = tmpfile();
    uint16_t jz[] = {0x1008, 0x3009, 0x5004, 0x200A, 0x7000};
    uint16_t adm[] = {0x6100, 0x7000};
    memset(&prog, 0, sizeof prog);
    memcpy(prog.code, jz, sizeof jz);
    prog.ncode = 5;
    prog.daddr[0] = 8;
    prog.dval[0] = 1;
    prog.daddr[1] = 9;
    prog.dval[1] = 0xFFFF;
    prog.ndata = 2;
    CHECK(cpu_exec(&prog, &cpu, fp) == 0);
    CHECK_EQ(cpu.acc, 0);
    CHECK_EQ(cpu.psw.zf, 1);
    CHECK_EQ(cpu.psw.cf, 1);
    CHECK_EQ(cpu.psw.of, 0);
    CHECK_EQ(cpu.dirty[10], 0);
    CHECK_EQ(cpu.pc, 5);
    memset(&prog, 0, sizeof prog);
    memcpy(prog.code, adm, sizeof adm);
    prog.ncode = 2;
    prog.reg[0] = 6;
    prog.reg[1] = 10;
    prog.reg_mask = 3;
    prog.daddr[0] = 10;
    prog.dval[0] = 4;
    prog.ndata = 1;
    CHECK(cpu_exec(&prog, &cpu, fp) == 0);
    CHECK_EQ(cpu.acc, 10);
    CHECK_EQ(cpu.mem[10], 10);
    CHECK_EQ(cpu.r[1], 10);
    CHECK_EQ(cpu.conflicts, 0);
    fclose(fp);
}

static void test_machine(void) {
    Cpu cpu;
    uint16_t nop[] = {0x0000, 0x7000};
    test_add();
    test_jz_adm();
    run_words(nop, 2, &cpu);
    CHECK_EQ(cpu.pc, 2);
    CHECK_EQ(cpu.visits, 2);
}

static void test_exam(void) {
    CHECK_EQ(bits_needed(7), 3);
    CHECK_EQ(bits_needed(3), 2);
    CHECK_EQ(micro_len_sub(), 4);
    CHECK_EQ(micro_len_inc(), 2);
    CHECK_EQ(fetch_clocks(5), 7);
    CHECK_EQ(gpr_count(4), 16);
    CHECK_EQ(addr_bits((1ULL << 32) / 4), 30);
    CHECK_EQ(addr_bits(1ULL << 32), 32);
    CHECK_EQ(min_machine_cycles(), 2);
    CHECK_EQ(reg_visible("pc"), 1);
    CHECK_EQ(reg_visible("mar"), 0);
    CHECK_EQ(reg_visible("ir"), 0);
    CHECK_EQ(reg_visible("base"), 1);
    CHECK_EQ(reg_visible("flag"), 1);
    CHECK_EQ(reg_visible("mir"), 0);
    CHECK_EQ(pick_letter("2009-11"), 'C');
    CHECK_EQ(pick_letter("2010-18"), 'B');
    CHECK_EQ(pick_letter("2011-19"), 'C');
    CHECK_EQ(pick_letter("2016-18"), 'B');
    CHECK_EQ(pick_letter("2016-20"), 'A');
    CHECK_EQ(pick_letter("2019-17"), 'B');
    CHECK_EQ(pick_letter("2021-17"), 'D');
    CHECK_EQ(pick_letter("2021-18"), 'C');
    CHECK_EQ(pick_letter("2023-18"), 'B');
}

int main(void) {
    test_bus();
    test_flags();
    test_encode();
    test_machine();
    test_exam();
    return TEST_SUMMARY();
}
