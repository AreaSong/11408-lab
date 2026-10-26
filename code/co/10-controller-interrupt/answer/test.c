#include "calc.h"
#include "ctrl.h"
#include "micro.h"
#include "sim.h"
#include "test.h"
#include "wire.h"

#include <stdio.h>

static void test_width(void) {
    int classic[] = {7, 3, 12, 5, 6};
    int power[] = {8, 4};
    int layout[] = {5, 8, 14, 3};
    int naive = 0;
    CHECK_EQ(ceil_log2(130), 8);
    CHECK_EQ(ceil_log2(128), 7);
    CHECK_EQ(ceil_log2(33), 6);
    CHECK_EQ(field_bits(8), 4);
    CHECK_EQ(field_bits(7), 3);
    CHECK_EQ(group_sum(classic, 5, &naive), 15);
    CHECK_EQ(naive, 15);
    CHECK_EQ(group_sum(power, 2, &naive), 7);
    CHECK_EQ(naive, 5);
    CHECK_EQ(group_sum(layout, 4, &naive), 13);
    CHECK_EQ(naive, 12);
}

static void test_wire(void) {
    CHECK_EQ(wire_sig(OP_LDA, 0, 0), SIG_PCOUT | SIG_MARIN);
    CHECK_EQ(wire_sig(OP_ADD, 1, 0), SIG_MEMR | SIG_MDRINE | SIG_PCPLUS);
    CHECK_EQ(wire_sig(OP_LDA, 3, 0), 0);
    CHECK_EQ(wire_sig(OP_DIV, 6, 1), SIG_SAVEPC | SIG_IFCLR | SIG_VEC);
    CHECK_EQ(wire_sig(OP_DIV, 7, 0), SIG_ACCOUT | SIG_DIV | SIG_ACCIN);
    CHECK_EQ(wire_sig(OP_TRAP, 4, 0), SIG_SAVEPC | SIG_IFCLR | SIG_VEC);
    CHECK_EQ(wire_sig(OP_IRET, 4, 0), SIG_SPCOUT | SIG_PCIN | SIG_IFSET);
    CHECK_EQ(wire_sig(OP_JMP, 4, 0), SIG_IROUT | SIG_PCIN);
    CHECK_EQ(wire_sig(OP_STA, 6, 0), SIG_MDROUTE | SIG_MEMW);
    CHECK_EQ(wire_sig(OP_HLT, 4, 0), 0);
    CHECK_EQ(out_count(wire_sig(OP_STA, 6, 0)), 0);
    CHECK_EQ(out_count(wire_sig(OP_ADD, 7, 0)), 1);
    CHECK_EQ(out_count(wire_sig(OP_LDA, 1, 0)), 0);
}

static void test_micro(void) {
    CHECK_EQ(micro_count(), 24);
    CHECK_EQ(micro_mismatches(), 0);
    CHECK_EQ(toy_direct_word(), 28);
    CHECK_EQ(toy_actual_bits(), 672);
    CHECK_EQ(toy_max_bits(), 896);
    CHECK_EQ(toy_field_control(), 20);
    CHECK_EQ(toy_field_word(), 26);
    CHECK_EQ(toy_field_actual(), 624);
    CHECK_EQ(toy_field_max(), 832);
}

static SceneResult run_named(const char *name) {
    SceneResult result;
    FILE *fp = tmpfile();
    int rc;
    result.halted = -1;
    if (fp == NULL) {
        return result;
    }
    rc = show_scene(fp, name, &result);
    fclose(fp);
    if (rc != 0) {
        result.halted = -1;
    }
    return result;
}

static void test_scenes(void) {
    SceneResult fault = run_named("fault");
    SceneResult trap = run_named("trap");
    SceneResult abort = run_named("abort");
    SceneResult irq = run_named("irq");
    SceneResult mask = run_named("mask");
    SceneResult nmi = run_named("nmi");
    CHECK_EQ(fault.acc, 4);
    CHECK_EQ(fault.pc, 4);
    CHECK_EQ(fault.saved, 1);
    CHECK_EQ(fault.took, 1);
    CHECK_EQ(fault.ie, 1);
    CHECK_EQ(fault.mem[20], 8);
    CHECK_EQ(fault.mem[21], 2);
    CHECK_EQ(fault.mem[22], 4);
    CHECK_EQ(fault.mem[24], 8);
    CHECK_EQ(trap.acc, 7);
    CHECK_EQ(trap.saved, 2);
    CHECK_EQ(trap.pc, 3);
    CHECK_EQ(trap.took, 1);
    CHECK_EQ(trap.ie, 1);
    CHECK_EQ(abort.pc, 0);
    CHECK_EQ(abort.saved, 0);
    CHECK_EQ(abort.took, 1);
    CHECK_EQ(abort.ie, 0);
    CHECK_EQ(abort.halted, 1);
    CHECK_EQ(irq.acc, 10);
    CHECK_EQ(irq.saved, 2);
    CHECK_EQ(irq.pc, 3);
    CHECK_EQ(irq.took, 1);
    CHECK_EQ(irq.ie, 1);
    CHECK_EQ(mask.took, 0);
    CHECK_EQ(mask.ie, 0);
    CHECK_EQ(mask.saved, 0);
    CHECK_EQ(mask.pc, 3);
    CHECK_EQ(nmi.took, 1);
    CHECK_EQ(nmi.ie, 1);
    CHECK_EQ(nmi.saved, 1);
    CHECK_EQ(nmi.pc, 2);
}

int main(void) {
    test_width();
    test_wire();
    test_micro();
    test_scenes();
    return TEST_SUMMARY();
}
