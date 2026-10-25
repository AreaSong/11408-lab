#include "boot.h"
#include "claim.h"
#include "guest_vm.h"
#include "mbr_image.h"
#include "struct_path.h"
#include "test.h"
#include "vmm_type.h"

#include <string.h>

static void expect_path_step(const PathStep *step, const char *from, const char *to, int sw,
                             const char *what) {
    CHECK_STR(step->from, from);
    CHECK_STR(step->to, to);
    CHECK_EQ(step->mode_switch, sw);
    CHECK_STR(step->what, what);
}

static void test_struct_catalog(void) {
    const char *facts = NULL;
    CHECK(struct_lookup("layered", &facts));
    CHECK_STR(facts, "calls=adjacent skip=no debug=easy efficiency=low");
    CHECK(struct_lookup("linux", &facts));
    CHECK_STR(facts, "kind=monolithic module=in-kernel ipc=call");
    CHECK(struct_lookup("hybrid", &facts));
    CHECK_STR(facts, "example=xnu micro=mach place=mixed");
    CHECK(!struct_lookup("nope", &facts));
}

static void test_monolithic_and_micro(void) {
    PathTrace trace;
    CHECK(path_simulate("monolithic", &trace));
    CHECK_EQ(trace.n, 3);
    CHECK_EQ(trace.switches, 2);
    CHECK_EQ(trace.messages, 0);
    CHECK_EQ(trace.calls, 1);
    expect_path_step(&trace.steps[1], "kernel", "fs", 0, "call");

    CHECK(path_simulate("micro", &trace));
    CHECK_EQ(trace.n, 4);
    CHECK_EQ(trace.switches, 4);
    CHECK_EQ(trace.messages, 2);
    CHECK_EQ(trace.calls, 0);
    expect_path_step(&trace.steps[1], "kernel", "file-server", 1, "forward");
    expect_path_step(&trace.steps[2], "file-server", "kernel", 1, "reply");
}

static void test_layered_and_exo(void) {
    PathTrace trace;
    int i;
    int skipped = 0;
    CHECK(path_simulate("layered", &trace));
    CHECK_EQ(trace.switches, 2);
    CHECK_EQ(trace.calls, 2);
    expect_path_step(&trace.steps[1], "syscall-layer", "fs-layer", 0, "call");
    expect_path_step(&trace.steps[2], "fs-layer", "device-layer", 0, "call");
    for (i = 0; i < trace.n; i++) {
        if (strcmp(trace.steps[i].from, "syscall-layer") == 0 &&
            strcmp(trace.steps[i].to, "device-layer") == 0) {
            skipped = 1;
        }
    }
    CHECK_EQ(skipped, 0);

    CHECK(path_simulate("exo", &trace));
    CHECK_EQ(trace.switches, 2);
    CHECK_EQ(trace.calls, 1);
    expect_path_step(&trace.steps[0], "user", "libos", 0, "call");
    expect_path_step(&trace.steps[1], "libos", "exokernel", 1, "check-raw");
    CHECK(!path_simulate("hybrid", &trace));
}

static void expect_boot(const BootStep *step, const char *who, const char *from, const char *to,
                        const char *what) {
    CHECK_STR(step->who, who);
    CHECK_STR(step->from, from);
    CHECK_STR(step->to, to);
    CHECK_STR(step->what, what);
}

static void test_boot_trace(void) {
    BootTrace trace;
    const char *when = NULL;
    CHECK(boot_trace(&trace));
    CHECK_EQ(trace.n, 7);
    expect_boot(&trace.steps[0], "firmware", "rom", "cpu", "post");
    expect_boot(&trace.steps[1], "firmware", "disk:0", "mem:0x7c00", "read-mbr");
    expect_boot(&trace.steps[2], "disk-boot", "mbr:table", "-", "find-active");
    expect_boot(&trace.steps[4], "part-boot", "partition", "mem:kernel", "load-kernel");
    expect_boot(&trace.steps[6], "os-init", "kernel", "user:init", "first-process");
    CHECK_STR(boot_program_order(), "firmware-disk-boot-part-boot-os-init");
    CHECK_STR(boot_prep_order(), "physical-format-partition-logical-format-install");
    CHECK(boot_artifact("ivt", &when));
    CHECK_STR(when, "os-init");
    CHECK(boot_artifact("root", &when));
    CHECK_STR(when, "logical-format");
    CHECK(boot_artifact("partition-table", &when));
    CHECK_STR(when, "partition");
    CHECK(boot_artifact("sector", &when));
    CHECK_STR(when, "physical-format");
    CHECK(!boot_artifact("kernel", &when));
}

static void test_claims_and_vmm(void) {
    const char *truth = NULL;
    const VmmFact *fact = NULL;
    CHECK(claim_lookup("micro-performance", &truth));
    CHECK_STR(truth, "false");
    CHECK(claim_lookup("micro-reliability", &truth));
    CHECK_STR(truth, "true");
    CHECK(claim_lookup("micro-security", &truth));
    CHECK_STR(truth, "true");
    CHECK(claim_lookup("micro-extensible", &truth));
    CHECK_STR(truth, "true");
    CHECK(claim_lookup("vmm-same-privilege", &truth));
    CHECK_STR(truth, "false");
    CHECK(claim_lookup("multi-isa", &truth));
    CHECK_STR(truth, "true");
    CHECK(claim_lookup("os-on-vm", &truth));
    CHECK_STR(truth, "true");
    CHECK(claim_lookup("many-vm", &truth));
    CHECK_STR(truth, "true");
    CHECK(!claim_lookup("faster", &truth));

    CHECK(vmm_lookup("type1", &fact));
    CHECK_STR(fact->where, "hardware");
    CHECK_STR(fact->host, "no");
    CHECK_STR(fact->privilege, "highest");
    CHECK(vmm_lookup("type2", &fact));
    CHECK_STR(fact->where, "host-os");
    CHECK_STR(fact->migrate, "easy");
    CHECK_STR(fact->privilege, "above-guest");
    CHECK_STR(fact->performance, "lower");
}

static void test_mbr_layout(void) {
    PartSpec parts[1];
    MbrImage image;
    parts[0].status = 0x80;
    parts[0].type = 0x83;
    parts[0].lba = 2048;
    parts[0].sectors = 102400;
    CHECK(mbr_build(parts, 1, 0x55, 0xaa, &image));
    CHECK(image.sig_ok);
    CHECK_EQ(image.raw[MBR_SIG_OFFSET], 0x55);
    CHECK_EQ(image.raw[MBR_SIG_OFFSET + 1], 0xaa);
    CHECK_EQ(image.raw[0], 0);
    CHECK_EQ(image.raw[MBR_CODE_SIZE - 1], 0);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET], 0x80);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 4], 0x83);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 1], 0);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 8], 0x00);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 9], 0x08);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 10], 0x00);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 11], 0x00);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 12], 0x00);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 13], 0x90);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 14], 0x01);
    CHECK_EQ(image.raw[MBR_TABLE_OFFSET + 15], 0x00);
    CHECK(image.entry[0].active);
    CHECK(image.entry[0].used);
    CHECK_EQ(image.entry[0].lba, 2048);
    CHECK_EQ(image.entry[0].sectors, 102400);
    CHECK(!image.entry[1].used);
    CHECK(!mbr_build(NULL, 5, 0x55, 0xaa, &image));
}

static void test_mbr_hand_bytes(void) {
    unsigned char raw[MBR_SIZE];
    MbrImage image;
    memset(raw, 0, sizeof raw);
    raw[MBR_SIG_OFFSET] = 0x55;
    raw[MBR_SIG_OFFSET + 1] = 0xaa;
    raw[MBR_TABLE_OFFSET] = 0x80;
    raw[MBR_TABLE_OFFSET + 4] = 0x83;
    raw[MBR_TABLE_OFFSET + 8] = 0x00;
    raw[MBR_TABLE_OFFSET + 9] = 0x08;
    CHECK(mbr_parse(raw, &image));
    CHECK(image.sig_ok);
    CHECK_EQ(image.entry[0].lba, 2048);
    raw[MBR_SIG_OFFSET + 1] = 0x00;
    CHECK(mbr_parse(raw, &image));
    CHECK(!image.sig_ok);
    CHECK_EQ(image.entry[0].lba, 2048);
}

static void test_guest_trap_and_popf(void) {
    const char *instrs[] = {"cli", "popf"};
    const char *opened[] = {"sti"};
    GuestTrace trace;
    const char *unknown = NULL;
    /* popf 不能把 cli 清掉的虚拟中断标志改回来，否则陷入-模拟就漏不掉。 */
    CHECK(guest_exec(instrs, 2, &trace, &unknown));
    CHECK(unknown == NULL);
    CHECK_EQ(trace.n, 2);
    CHECK_EQ(trace.traps, 1);
    CHECK_EQ(trace.missed, 1);
    CHECK_EQ(trace.virt_if, 0);
    CHECK_STR(trace.steps[0].actor, "hypervisor");
    CHECK_EQ(trace.steps[0].trap, 1);
    CHECK_STR(trace.steps[1].klass, "sensitive");
    CHECK_EQ(trace.steps[1].trap, 0);
    CHECK_STR(trace.steps[1].effect, "missed");
    CHECK_STR(trace.steps[1].actor, "guest");
    CHECK_STR(trace.stopped, "end");

    CHECK(guest_exec(opened, 1, &trace, &unknown));
    CHECK_EQ(trace.virt_if, 1);
    CHECK_STR(trace.steps[0].effect, "if=1");
}

static void test_guest_halt_and_unknown(void) {
    const char *halted[] = {"mov", "hlt", "add"};
    const char *bad[] = {"mov", "foo"};
    GuestTrace trace;
    const char *unknown = NULL;
    CHECK(guest_exec(halted, 3, &trace, &unknown));
    CHECK_EQ(trace.n, 2);
    CHECK_EQ(trace.halted, 1);
    CHECK_STR(trace.stopped, "halt");
    CHECK_STR(trace.steps[0].effect, "execute");
    CHECK_EQ(trace.steps[0].trap, 0);

    CHECK(!guest_exec(bad, 2, &trace, &unknown));
    CHECK_EQ(trace.n, 1);
    CHECK_STR(unknown, "foo");
}

int main(void) {
    test_struct_catalog();
    test_monolithic_and_micro();
    test_layered_and_exo();
    test_boot_trace();
    test_claims_and_vmm();
    test_mbr_layout();
    test_mbr_hand_bytes();
    test_guest_trap_and_popf();
    test_guest_halt_and_unknown();
    return TEST_SUMMARY();
}
