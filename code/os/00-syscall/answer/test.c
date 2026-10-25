#include "claim.h"
#include "event.h"
#include "linkload.h"
#include "mode.h"
#include "syscall_path.h"
#include "test.h"

static void expect_mode(const char *name, const char *want) {
    const char *got = NULL;
    CHECK(mode_lookup(name, &got));
    CHECK_STR(got, want);
}

static void test_user_mode_instructions(void) {
    expect_mode("trap", "user");
    expect_mode("jump", "user");
    expect_mode("push", "user");
    expect_mode("mov", "user");
    expect_mode("sin", "user");
    expect_mode("compiler", "user");
    expect_mode("linker", "user");
    expect_mode("shell", "user");
}

static void test_kernel_mode_instructions(void) {
    expect_mode("cli", "kernel");
    expect_mode("sti", "kernel");
    expect_mode("halt", "kernel");
    expect_mode("io", "kernel");
    expect_mode("iret", "kernel");
    expect_mode("set-clock", "kernel");
    expect_mode("clear-memory", "kernel");
    expect_mode("loader", "kernel");
    expect_mode("scheduler", "kernel");
    expect_mode("process-switch", "kernel");
    expect_mode("page-fault-handler", "kernel");
}

static void test_occur_and_switch(void) {
    const char *yes = NULL;
    CHECK(occur_in_user("syscall", &yes));
    CHECK_STR(yes, "yes");
    CHECK(occur_in_user("external-interrupt", &yes));
    CHECK_STR(yes, "yes");
    CHECK(occur_in_user("page-fault", &yes));
    CHECK_STR(yes, "yes");
    CHECK(occur_in_user("process-switch", &yes));
    CHECK_STR(yes, "no");

    CHECK(causes_kernel_switch("div-zero", &yes));
    CHECK_STR(yes, "yes");
    CHECK(causes_kernel_switch("read", &yes));
    CHECK_STR(yes, "yes");
    CHECK(causes_kernel_switch("sin", &yes));
    CHECK_STR(yes, "no");
    CHECK(causes_kernel_switch("jump", &yes));
    CHECK_STR(yes, "no");
}

static void test_mode_rule(void) {
    const char *yes = NULL;
    CHECK(mode_can("user", "privileged", &yes));
    CHECK_STR(yes, "no");
    CHECK(mode_can("user", "unprivileged", &yes));
    CHECK_STR(yes, "yes");
    CHECK(mode_can("kernel", "privileged", &yes));
    CHECK_STR(yes, "yes");
    CHECK(mode_can("kernel", "unprivileged", &yes));
    CHECK_STR(yes, "yes");
    CHECK(!mode_can("core", "privileged", &yes));
}

static void test_claims(void) {
    const char *truth = NULL;
    CHECK(claim_lookup("unified-abi", &truth));
    CHECK_STR(truth, "false");
    CHECK(claim_lookup("event-time-always-kernel", &truth));
    CHECK_STR(truth, "false");
    CHECK(claim_lookup("service-in-kernel", &truth));
    CHECK_STR(truth, "true");
    CHECK(claim_lookup("handler-starts-in-kernel", &truth));
    CHECK_STR(truth, "true");
    CHECK(!claim_lookup("nope", &truth));
    CHECK_STR(truth, "true");
}

static void expect_event(const char *name, const char *klass, const char *subtype,
                         const char *ret_to) {
    const char *got_klass = NULL;
    const char *got_subtype = NULL;
    const char *got_ret = NULL;
    CHECK(event_lookup(name, &got_klass, &got_subtype, &got_ret));
    CHECK_STR(got_klass, klass);
    CHECK_STR(got_subtype, subtype);
    CHECK_STR(got_ret, ret_to);
}

static void test_events(void) {
    expect_event("keyboard", "interrupt", "external", "next");
    expect_event("clock", "interrupt", "external", "next");
    expect_event("page-fault", "exception", "fault", "current");
    expect_event("float-underflow", "exception", "fault", "current");
    expect_event("div-zero", "exception", "fault", "none");
    expect_event("illegal", "exception", "fault", "none");
    expect_event("protection", "exception", "fault", "none");
    expect_event("controller-error", "exception", "abort", "none");
    expect_event("memory-check", "exception", "abort", "none");
    expect_event("syscall", "exception", "trap", "next");
    expect_event("breakpoint", "exception", "trap", "next");
    const char *klass = "keep";
    CHECK(!event_lookup("nope", &klass, &klass, &klass));
    CHECK_STR(klass, "keep");
}

static void test_syscall_steps(void) {
    CHECK_STR(syscall_order(), "3-2-4-1");
    CHECK_EQ(syscall_step_count(), 4);
    CHECK(syscall_step_at(-1) == NULL);
    CHECK(syscall_step_at(4) == NULL);

    const SyscallStep *pass = syscall_step_at(0);
    CHECK_EQ(pass->label, 3);
    CHECK_STR(pass->actor, "user");
    CHECK_STR(pass->saves, "-");

    const SyscallStep *trap = syscall_step_at(1);
    CHECK_EQ(trap->label, 2);
    CHECK_STR(trap->actor, "hardware");
    CHECK_STR(trap->saves, "pc,psw");
    CHECK_STR(trap->mode, "user-to-kernel");

    const SyscallStep *service = syscall_step_at(2);
    CHECK_EQ(service->label, 4);
    CHECK_STR(service->actor, "os");
    CHECK_STR(service->saves, "general-regs");

    const SyscallStep *back = syscall_step_at(3);
    CHECK_EQ(back->label, 1);
    CHECK_STR(back->actor, "os");
    CHECK_STR(back->mode, "kernel-to-user");
}

static void test_link_and_load(void) {
    const LinkFact *link = NULL;
    CHECK(link_lookup("static", &link));
    CHECK_STR(link->when, "before-run");
    CHECK_STR(link->key, "unpack");
    CHECK_STR(link->value, "no");
    CHECK(link_lookup("run-time", &link));
    CHECK_STR(link->key, "unused");
    CHECK_STR(link->value, "skip");
    CHECK(!link_lookup("nope", &link));

    const LoadFact *load = NULL;
    CHECK(load_lookup("absolute", &load));
    CHECK_STR(load->when, "compile");
    CHECK_STR(load->movable, "no");
    CHECK(load_lookup("dynamic", &load));
    CHECK_STR(load->address, "relocation-register");
    CHECK_STR(load->movable, "yes");
    CHECK(load_lookup("relocatable", &load));
    CHECK_STR(load->address, "patched-once");
}

static void test_unknown_leaves_output(void) {
    const char *mode = "stale";
    CHECK(!mode_lookup("nope", &mode));
    CHECK_STR(mode, "stale");
    CHECK(!mode_lookup(NULL, &mode));
}

int main(void) {
    test_user_mode_instructions();
    test_kernel_mode_instructions();
    test_occur_and_switch();
    test_mode_rule();
    test_claims();
    test_events();
    test_syscall_steps();
    test_link_and_load();
    test_unknown_leaves_output();
    return TEST_SUMMARY();
}
