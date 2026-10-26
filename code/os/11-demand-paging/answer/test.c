#include "alloc.h"
#include "exam.h"
#include "fetch.h"
#include "origin.h"
#include "slot.h"
#include "test.h"
#include "vm.h"

static void test_vm_flow(void) {
    Vm vm;
    VmEvent ev;

    vm_reset(&vm, 2);
    CHECK_EQ(vm_define(&vm, 0, 100, 1), 0);
    CHECK_EQ(vm_define(&vm, 0, 100, 1), -2);
    CHECK_EQ(vm_define(&vm, 1, 200, 1), 0);
    CHECK_EQ(vm_define(&vm, 2, 300, 0), 0);

    CHECK_EQ(vm_touch(&vm, 0, 0, &ev), 0);
    CHECK_EQ(ev.kind, VM_FAULT);
    CHECK_EQ(ev.victim, -1);
    CHECK_EQ(ev.frame, 0);
    CHECK_EQ(ev.dirty, 0);

    CHECK_EQ(vm_touch(&vm, 0, 1, &ev), 0);
    CHECK_EQ(ev.kind, VM_HIT);
    CHECK_EQ(vm.page[0].dirty, 1);

    CHECK_EQ(vm_touch(&vm, 1, 1, &ev), 0);
    CHECK_EQ(ev.kind, VM_FAULT);
    CHECK_EQ(ev.frame, 1);
    CHECK_EQ(ev.dirty, 1);

    CHECK_EQ(vm_touch(&vm, 2, 1, &ev), 0);
    CHECK_EQ(ev.kind, VM_ABORT);
    CHECK_EQ(ev.reason, 2);
    CHECK_EQ(vm.page[2].present, 0);

    CHECK_EQ(vm_touch(&vm, 2, 0, &ev), 0);
    CHECK_EQ(ev.kind, VM_FAULT);
    CHECK_EQ(ev.victim, 0);
    CHECK_EQ(ev.victim_dirty, 1);
    CHECK_EQ(ev.victim_disk, 100);
    CHECK_EQ(vm.page[0].present, 0);
    CHECK_EQ(vm.page[0].dirty, 0);
    CHECK_EQ(vm.page[0].disk, 100);
    CHECK_EQ(vm.page[2].present, 1);
    CHECK_EQ(vm.page[2].frame, 0);
    CHECK_EQ(vm.page[2].dirty, 0);

    CHECK_EQ(vm_touch(&vm, 9, 0, &ev), 0);
    CHECK_EQ(ev.kind, VM_ABORT);
    CHECK_EQ(ev.reason, 1);
}

static void test_vm_discard(void) {
    Vm vm;
    VmEvent ev;

    vm_reset(&vm, 1);
    CHECK_EQ(vm_define(&vm, 0, 10, 1), 0);
    CHECK_EQ(vm_define(&vm, 1, 11, 1), 0);
    CHECK_EQ(vm_touch(&vm, 0, 0, &ev), 0);
    CHECK_EQ(vm_touch(&vm, 1, 0, &ev), 0);
    CHECK_EQ(ev.victim, 0);
    CHECK_EQ(ev.victim_dirty, 0);
    CHECK_EQ(vm.page[1].frame, 0);
}

static int run_alloc(int policy, int *f0, int *r0, int *c0, int *f1, int *r1, int *c1) {
    static const int pid[] = {1, 1, 0, 0, 0, 0, 0, 1};
    static const int page[] = {7, 8, 1, 2, 3, 1, 4, 7};
    int base[] = {2, 2};
    Alloc alloc;
    int i;

    if (alloc_init(&alloc, policy, 5, 2, base) != 0) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        AllocRef ref;
        if (alloc_ref(&alloc, pid[i], page[i], &ref) != 0) {
            return -1;
        }
    }
    *f0 = alloc.proc[0].faults;
    *r0 = alloc.proc[0].replaces;
    *c0 = alloc.proc[0].cap;
    *f1 = alloc.proc[1].faults;
    *r1 = alloc.proc[1].replaces;
    *c1 = alloc.proc[1].cap;
    return alloc.pool;
}

static void test_alloc_policies(void) {
    int f0, r0, c0, f1, r1, c1;
    int legal = 1;

    CHECK_EQ(run_alloc(ALLOC_FIXED_LOCAL, &f0, &r0, &c0, &f1, &r1, &c1), 1);
    CHECK_EQ(f0, 5);
    CHECK_EQ(r0, 3);
    CHECK_EQ(c0, 2);
    CHECK_EQ(f1, 2);
    CHECK_EQ(r1, 0);
    CHECK_EQ(c1, 2);

    CHECK_EQ(run_alloc(ALLOC_VAR_GLOBAL, &f0, &r0, &c0, &f1, &r1, &c1), 0);
    CHECK_EQ(f0, 4);
    CHECK_EQ(r0, 1);
    CHECK_EQ(c0, 4);
    CHECK_EQ(f1, 3);
    CHECK_EQ(r1, 1);
    CHECK_EQ(c1, 1);

    CHECK_EQ(run_alloc(ALLOC_VAR_LOCAL, &f0, &r0, &c0, &f1, &r1, &c1), 0);
    CHECK_EQ(f0, 4);
    CHECK_EQ(r0, 1);
    CHECK_EQ(c0, 3);
    CHECK_EQ(f1, 2);
    CHECK_EQ(r1, 0);
    CHECK_EQ(c1, 2);

    CHECK_EQ(alloc_combo("fixed-global", &legal), 0);
    CHECK_EQ(legal, 0);
    CHECK_EQ(alloc_combo("fixed-local", &legal), 0);
    CHECK_EQ(legal, 1);
    CHECK_EQ(alloc_combo("no-such", &legal), -1);
}

static void test_fetch(void) {
    int seq[] = {0, 1, 2, 3, 0, 1};
    int jump[] = {0, 2, 0, 2};
    int faults = 0;
    int extra = 0;

    CHECK_EQ(fetch_run(4, 0, 6, seq, 6, &faults, &extra), 0);
    CHECK_EQ(faults, 4);
    CHECK_EQ(extra, 0);
    CHECK_EQ(fetch_run(4, 1, 6, seq, 6, &faults, &extra), 0);
    CHECK_EQ(faults, 2);
    CHECK_EQ(extra, 2);
    CHECK_EQ(fetch_run(2, 0, 6, jump, 4, &faults, &extra), 0);
    CHECK_EQ(faults, 2);
    CHECK_EQ(fetch_run(2, 1, 6, jump, 4, &faults, &extra), 0);
    CHECK_EQ(faults, 3);
    CHECK_EQ(extra, 1);
}

static void test_2024_slot(void) {
    unsigned long long page = 0;
    unsigned long long offset = 0;
    unsigned long long addr = 0;
    const unsigned long long page_bytes = 0x400000ULL;

    CHECK_EQ(addr_split(page_bytes, 0x12345678ULL, &page, &offset), 0);
    CHECK_EQ(page, 0x48ULL);
    CHECK_EQ(offset, 0x345678ULL);
    CHECK_EQ(pte_addr(page_bytes, 4, 0xB8C00000ULL, page, &addr), 0);
    CHECK_EQ(addr, 0xB8C00120ULL);
    CHECK_EQ(pte_addr(page_bytes, 4, 0x65400000ULL, page, &addr), 0);
    CHECK_EQ(addr, 0x65400120ULL);
    CHECK_EQ(addr_split(page_bytes, 0xBAB45678ULL, &page, &offset), 0);
    CHECK_EQ(page, 0x2EAULL);
    CHECK_EQ(addr_split(page_bytes, 0xB8C00000ULL, &page, &offset), 0);
    CHECK_EQ(page, 0x2E3ULL);
    CHECK_EQ(pte_addr(page_bytes, 4, 0xB8C00000ULL, 0x2E3ULL, &addr), 0);
    CHECK_EQ(addr, 0xB8C00B8CULL);
    CHECK_EQ(addr_split(page_bytes, 0x65400000ULL, &page, &offset), 0);
    CHECK_EQ(page, 0x195ULL);
}

static void test_origin_and_exam(void) {
    Origin origin;
    char letter = '?';
    const char *note = 0;

    CHECK_EQ(origin_of("poor", "clean", &origin), 0);
    CHECK_STR(origin.from, "file");
    CHECK_STR(origin.evict, "discard");
    CHECK_EQ(origin_of("poor", "dirty-first", &origin), 0);
    CHECK_STR(origin.from, "file");
    CHECK_STR(origin.evict, "swap");
    CHECK_EQ(origin_of("poor", "dirty-next", &origin), 0);
    CHECK_STR(origin.from, "swap");
    CHECK_EQ(origin_of("rich", "clean", &origin), 0);
    CHECK_STR(origin.from, "swap");
    CHECK_EQ(origin_of("unix", "shared", &origin), 0);
    CHECK_STR(origin.from, "memory");
    CHECK_STR(origin.evict, "none");

    CHECK_EQ(exam_letter("2019-14", &letter, &note), 0);
    CHECK_EQ(letter, 'D');
    CHECK(note == 0);
    CHECK_EQ(exam_letter("2012-25", &letter, &note), 0);
    CHECK_EQ(letter, 'B');
    CHECK_EQ(exam_letter("2015-30", &letter, &note), 0);
    CHECK_EQ(letter, 'C');
    CHECK_EQ(exam_letter("2025-30", &letter, &note), 0);
    CHECK_EQ(letter, 'A');
    CHECK_STR(note, "ii-disputed");
    CHECK_EQ(exam_letter("1999-1", &letter, &note), -1);
}

int main(void) {
    test_vm_flow();
    test_vm_discard();
    test_alloc_policies();
    test_fetch();
    test_2024_slot();
    test_origin_and_exam();
    return TEST_SUMMARY();
}
