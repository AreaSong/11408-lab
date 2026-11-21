#include "kernel/k_buddy.h"
#include "kernel/k_vm.h"
#include "test.h"

#include <string.h>

void test_live(void);

static void test_buddy(void) {
    KBuddy b;
    KBuddyBlk blk[8];
    int at = -1;
    int size = -1;
    int merges = -1;
    CHECK(k_buddy_open(&b, 16) == 0);
    CHECK(k_buddy_links(&b));
    CHECK(k_buddy_alloc(&b, "A", 3, &at, &size) == 0);
    CHECK_EQ(at, 0);
    CHECK_EQ(size, 4);
    CHECK(k_buddy_links(&b));
    CHECK(k_buddy_free(&b, "A", &merges) == 0);
    CHECK_EQ(merges, 2);
    CHECK_EQ(k_buddy_list(&b, 1, blk, 8), 1);
    CHECK_EQ(blk[0].start, 0);
    CHECK_EQ(blk[0].size, 16);
    CHECK(k_buddy_alloc(&b, "A", 4, &at, &size) == 0);
    CHECK(k_buddy_alloc(&b, "B", 4, &at, &size) == 0);
    CHECK_EQ(at, 4);
    CHECK(k_buddy_alloc(&b, "C", 4, &at, &size) == 0);
    CHECK_EQ(at, 8);
    CHECK(k_buddy_alloc(&b, "D", 4, &at, &size) == 0);
    CHECK_EQ(at, 12);
    CHECK(k_buddy_free(&b, "B", &merges) == 0);
    CHECK_EQ(merges, 0);
    CHECK(k_buddy_free(&b, "C", &merges) == 0);
    CHECK_EQ(merges, 0);
    CHECK_EQ(k_buddy_alloc(&b, "Z", 100, &at, &size), K_BUDDY_TOO_BIG);
    k_buddy_close(&b);
}

static int faults_of(const char *algo, int frames, const int *pages, int n) {
    KAccess refs[K_VM_REFS];
    KReport report;
    int i;
    for (i = 0; i < n; i++) {
        refs[i].page = pages[i];
        refs[i].write = 0;
    }
    CHECK(k_repl_run(algo, frames, NULL, 0, refs, n, &report) == 0);
    return report.faults;
}

static void test_tang(void) {
    static const int tang[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    static const int fifo_f[] = {20, 15, 15, 10, 9, 6};
    static const int lru_f[] = {20, 17, 12, 8, 7, 6};
    static const int opt_f[] = {20, 13, 9, 8, 7, 6};
    static const int clock_f[] = {20, 15, 14, 9, 9, 6};
    static const int opt_victim[] = {7, 1, 0, 4, 3, 2};
    KReport report;
    KAccess refs[20];
    int got[6];
    int n = 0;
    int i;
    int frames;
    int oldest = -1;
    for (frames = 1; frames <= 6; frames++) {
        CHECK_EQ(faults_of("fifo", frames, tang, 20), fifo_f[frames - 1]);
        CHECK_EQ(faults_of("lru", frames, tang, 20), lru_f[frames - 1]);
        CHECK_EQ(faults_of("opt", frames, tang, 20), opt_f[frames - 1]);
        CHECK_EQ(faults_of("clock", frames, tang, 20), clock_f[frames - 1]);
    }
    CHECK_EQ(faults_of("clock2", 3, tang, 20), 14);
    for (i = 0; i < 20; i++) {
        refs[i].page = tang[i];
        refs[i].write = 0;
    }
    CHECK(k_repl_run("opt", 3, NULL, 0, refs, 20, &report) == 0);
    for (i = 0; i < report.n; i++) {
        if (report.step[i].victim >= 0) {
            got[n++] = report.step[i].victim;
        }
    }
    CHECK_EQ(n, 6);
    for (i = 0; i < 6; i++) {
        CHECK_EQ(got[i], opt_victim[i]);
    }
    CHECK(k_lru_demo(&oldest));
    CHECK_EQ(oldest, 2);
}

static void test_exam_and_clock(void) {
    char buf[96];
    int page[] = {0, 1, 2};
    int aref[] = {1, 0, 0};
    int dirty[] = {0, 0, 1};
    int longp[] = {0, 1, 2, 3, 4};
    int along[] = {1, 1, 0, 1, 1};
    int dlong[] = {1, 0, 1, 1, 0};
    KReport simple;
    KReport better;
    int uniq[8];
    int nu = 0;
    static const int y2016[] = {1, 3, 4, 5, 6, 0, 3, 2, 3, 2};
    CHECK(k_vm_exam("2019-29", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "answer=C") != NULL);
    CHECK(k_vm_exam("2015-27", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "answer=A") != NULL);
    CHECK(k_vm_exam("2025-26", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "answer=B") != NULL);
    CHECK(k_vm_exam("2014-30", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "answer=A") != NULL);
    CHECK(k_vm_exam("2015-30", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "answer=C") != NULL);
    CHECK(k_ws(y2016, 10, 6, uniq, &nu) == 0);
    CHECK_EQ(nu, 4);
    CHECK_EQ(uniq[0], 0);
    CHECK_EQ(uniq[3], 6);
    CHECK(k_clock_once("clock", page, aref, dirty, 3, 9, 0, &simple) == 0);
    CHECK(k_clock_once("clock2", page, aref, dirty, 3, 9, 0, &better) == 0);
    CHECK_EQ(simple.step[0].victim, 1);
    CHECK_EQ(better.step[0].victim, 1);
    CHECK_EQ(better.step[0].passes, 1);
    CHECK_EQ(simple.step[0].refbit[0], 0);
    CHECK_EQ(better.step[0].refbit[0], 1);
    CHECK_EQ(better.step[0].hand, 2);
    CHECK(k_clock_once("clock2", longp, along, dlong, 5, 9, 0, &better) == 0);
    CHECK_EQ(better.step[0].victim, 2);
    CHECK_EQ(better.step[0].passes, 2);
}

static void test_alloc_fetch(void) {
    int base[] = {2, 2};
    int pid[] = {1, 1, 0, 0, 0, 0, 0, 1};
    int page[] = {7, 8, 1, 2, 3, 1, 4, 7};
    int faults = 0;
    int replaces = 0;
    int pref = 0;
    int seq_a[] = {0, 1, 2, 3, 0, 1};
    int seq_b[] = {0, 2, 0, 2};
    static const int thrash[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 3, 4, 5, 3, 4, 5, 3, 4, 5, 3, 4, 5};
    int jump = 0;
    CHECK(k_alloc_run("fixed-local", 5, base, 2, pid, page, 8, &faults, &replaces, NULL) == 0);
    CHECK_EQ(faults, 7);
    CHECK_EQ(replaces, 3);
    CHECK(k_alloc_run("var-global", 5, base, 2, pid, page, 8, &faults, &replaces, NULL) == 0);
    CHECK_EQ(faults, 7);
    CHECK_EQ(replaces, 2);
    CHECK(k_alloc_run("var-local", 5, base, 2, pid, page, 8, &faults, &replaces, NULL) == 0);
    CHECK_EQ(faults, 6);
    CHECK_EQ(replaces, 1);
    CHECK_EQ(k_alloc_id("fixed-global"), -2);
    CHECK(k_fetch_run(0, 4, 0, 6, seq_a, 6, &faults, &pref) == 0);
    CHECK_EQ(faults, 4);
    CHECK_EQ(pref, 0);
    CHECK(k_fetch_run(1, 4, 1, 6, seq_a, 6, &faults, &pref) == 0);
    CHECK_EQ(faults, 2);
    CHECK_EQ(pref, 2);
    CHECK(k_fetch_run(0, 2, 0, 6, seq_b, 4, &faults, &pref) == 0);
    CHECK_EQ(faults, 2);
    CHECK(k_fetch_run(1, 2, 1, 6, seq_b, 4, &faults, &pref) == 0);
    CHECK_EQ(faults, 3);
    CHECK_EQ(pref, 1);
    CHECK(k_thrash_jump("lru", thrash, 24, &jump) == 0);
    CHECK_EQ(jump, 3);
    CHECK_EQ(faults_of("lru", 3, thrash, 24), 6);
    CHECK_EQ(faults_of("lru", 2, thrash, 24), 24);
}

static void test_belady(void) {
    static const int want[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    KBelady found;
    int i;
    CHECK(k_belady(&found) == 0);
    CHECK_EQ(found.len, 12);
    for (i = 0; i < 12; i++) {
        CHECK_EQ(found.refs[i], want[i]);
    }
    CHECK_EQ(found.faults[K_REPL_FIFO][0], 9);
    CHECK_EQ(found.faults[K_REPL_FIFO][1], 10);
    CHECK(found.faults[K_REPL_LRU][0] >= found.faults[K_REPL_LRU][1]);
    CHECK(found.faults[K_REPL_OPT][0] >= found.faults[K_REPL_OPT][1]);
    CHECK_EQ(found.faults[K_REPL_CLOCK][0], 9);
    CHECK_EQ(found.faults[K_REPL_CLOCK][1], 10);
    CHECK_EQ(found.faults[K_REPL_CLOCK2][0], 9);
    CHECK_EQ(found.faults[K_REPL_CLOCK2][1], 10);
}

static void test_pte(void) {
    unsigned long long page = 0;
    unsigned long long off = 0;
    unsigned long long addr = 0;
    CHECK(k_addr_split(0x400000ull, 0x12345678ull, &page, &off) == 0);
    CHECK_EQ((int)page, 0x48);
    CHECK_EQ((int)off, 0x345678);
    CHECK(k_pte_addr(0x400000ull, 4, 0xb8c00000ull, 0x48ull, &addr) == 0);
    CHECK(addr == 0xb8c00120ull);
}

int main(void) {
    test_buddy();
    test_tang();
    test_exam_and_clock();
    test_alloc_fetch();
    test_belady();
    test_pte();
    test_live();
    return TEST_SUMMARY();
}
