#include "belady.h"
#include "io.h"
#include "order.h"
#include "sim.h"
#include "test.h"
#include "wset.h"

#include <string.h>

static void fill(Access *refs, const int *pages, int n) {
    int i;
    for (i = 0; i < n; i++) {
        refs[i].page = pages[i];
        refs[i].write = 0;
    }
}

static int faults_of(const char *algo, int frames, const int *pages, int n) {
    Access refs[REF_MAX];
    Report r;
    fill(refs, pages, n);
    CHECK(sim_exec(algo, frames, NULL, 0, refs, n, &r) == 0);
    return r.faults;
}

static void test_order(void) {
    Order lru;
    Order fifo;
    int victim = -1;

    CHECK(order_open(&lru, 1));
    CHECK(order_admit(&lru, 1));
    CHECK(order_admit(&lru, 2));
    CHECK(order_links_ok(&lru));
    CHECK_EQ(order_at(&lru, 1), 2);
    CHECK_EQ(order_at(&lru, 2), 1);
    CHECK(order_hit(&lru, 1));
    CHECK_EQ(order_at(&lru, 1), 1);
    CHECK_EQ(order_at(&lru, 2), 2);
    CHECK(order_evict(&lru, &victim));
    CHECK_EQ(victim, 2);
    CHECK(order_has(&lru, 1));
    CHECK(!order_has(&lru, 2));
    CHECK(!order_admit(&lru, 1));
    order_close(&lru);

    CHECK(order_open(&fifo, 0));
    CHECK(order_admit(&fifo, 1));
    CHECK(order_admit(&fifo, 2));
    CHECK(order_hit(&fifo, 1));
    CHECK_EQ(order_at(&fifo, 1), 1);
    CHECK(order_evict(&fifo, &victim));
    CHECK_EQ(victim, 1);
    order_close(&fifo);
    CHECK(clock_ring_ok());
}

static void test_split(void) {
    static const int pages[] = {1, 2, 3, 1, 4};
    Access refs[5];
    Report fifo;
    Report lru;

    fill(refs, pages, 5);
    CHECK(sim_exec("fifo", 3, NULL, 0, refs, 5, &fifo) == 0);
    CHECK(sim_exec("lru", 3, NULL, 0, refs, 5, &lru) == 0);
    CHECK_EQ(fifo.step[4].victim, 1);
    CHECK_EQ(lru.step[4].victim, 2);
    CHECK_EQ(lru.step[4].order[0], 4);
}

static void expect_faults(const char *algo, const int *pages, int n, const int *want) {
    int frames;
    for (frames = 1; frames <= 6; frames++) {
        CHECK_EQ(faults_of(algo, frames, pages, n), want[frames - 1]);
    }
}

static void test_tang(void) {
    static const int tang[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    static const int fifo_f[] = {20, 15, 15, 10, 9, 6};
    static const int lru_f[] = {20, 17, 12, 8, 7, 6};
    static const int opt_f[] = {20, 13, 9, 8, 7, 6};
    static const int clock_f[] = {20, 15, 14, 9, 9, 6};
    static const int opt_victim[] = {7, 1, 0, 4, 3, 2};
    Access refs[20];
    Report opt;
    int got[6];
    int n = 0;
    int i;

    expect_faults("fifo", tang, 20, fifo_f);
    expect_faults("lru", tang, 20, lru_f);
    expect_faults("opt", tang, 20, opt_f);
    expect_faults("clock", tang, 20, clock_f);
    fill(refs, tang, 20);
    CHECK(sim_exec("opt", 3, NULL, 0, refs, 20, &opt) == 0);
    CHECK_EQ(opt.faults, 9);
    CHECK_EQ(opt.replaces, 6);
    for (i = 0; i < opt.n; i++) {
        if (opt.step[i].victim >= 0) {
            got[n++] = opt.step[i].victim;
        }
    }
    CHECK_EQ(n, 6);
    for (i = 0; i < 6; i++) {
        CHECK_EQ(got[i], opt_victim[i]);
    }
}

static void victims_of(const Report *r, int *got, int *n) {
    int i;

    *n = 0;
    for (i = 0; i < r->n; i++) {
        if (r->step[i].victim >= 0) {
            got[(*n)++] = r->step[i].victim;
        }
    }
}

static void test_lru_exams(void) {
    static const int y2015[] = {2, 0, 2, 9, 3, 4, 2, 8, 2, 4, 8, 4, 5, 7};
    static const int y2019[] = {0, 1, 2, 7, 0, 5, 3, 5, 0, 2, 7, 6};
    static const int y2019_victim[] = {1, 2, 7, 3, 5};
    static const int y2025[] = {0, 1, 2, 0, 5, 1, 4, 3, 0, 2, 3, 2, 0};
    static const int y2025_victim[] = {1, 2, 0, 5, 1, 4};
    Access refs[16];
    Access warm[3] = {{0, 0}, {1, 0}, {2, 0}};
    Report r;
    int got[8];
    int n = 0;
    int i;

    fill(refs, y2015, 14);
    CHECK(sim_exec("lru", 4, NULL, 0, refs, 14, &r) == 0);
    CHECK_EQ(r.step[13].victim, 2);

    fill(refs, y2019, 12);
    CHECK(sim_exec("lru", 4, NULL, 0, refs, 12, &r) == 0);
    CHECK_EQ(r.faults, 9);
    CHECK_EQ(r.replaces, 5);
    CHECK_EQ(r.step[11].slots[0], 0);
    CHECK_EQ(r.step[11].slots[1], 6);
    CHECK_EQ(r.step[11].slots[2], 7);
    CHECK_EQ(r.step[11].slots[3], 2);
    victims_of(&r, got, &n);
    CHECK_EQ(n, 5);
    for (i = 0; i < 5; i++) {
        CHECK_EQ(got[i], y2019_victim[i]);
    }

    fill(refs, y2025, 13);
    CHECK(sim_exec("lru", 3, warm, 3, refs, 13, &r) == 0);
    CHECK_EQ(r.faults, 6);
    victims_of(&r, got, &n);
    CHECK_EQ(n, 6);
    for (i = 0; i < 6; i++) {
        CHECK_EQ(got[i], y2025_victim[i]);
    }
}

static void test_work_exam(void) {
    static const int y2016[] = {1, 3, 4, 5, 6, 0, 3, 2, 3, 2};
    int uniq[8];
    int nu = 0;

    CHECK(work_set(y2016, 10, 6, uniq, &nu) == 0);
    CHECK_EQ(nu, 4);
    CHECK_EQ(uniq[0], 0);
    CHECK_EQ(uniq[1], 2);
    CHECK_EQ(uniq[2], 3);
    CHECK_EQ(uniq[3], 6);
}

static void test_clock2(void) {
    int page[] = {0, 1, 2};
    int aref[] = {1, 0, 0};
    int dirty[] = {0, 0, 1};
    Access next = {9, 0};
    Report simple;
    Report better;
    int longp[] = {0, 1, 2, 3, 4};
    int along[] = {1, 1, 0, 1, 1};
    int dlong[] = {1, 0, 1, 1, 0};

    CHECK(clock_preset("clock", page, aref, dirty, 3, next, &simple) == 0);
    CHECK(clock_preset("clock2", page, aref, dirty, 3, next, &better) == 0);
    CHECK_EQ(simple.step[0].victim, 1);
    CHECK_EQ(better.step[0].victim, 1);
    CHECK_EQ(better.step[0].passes, 1);
    CHECK_EQ(simple.step[0].refbit[0], 0);
    CHECK_EQ(better.step[0].refbit[0], 1);
    CHECK_EQ(better.step[0].hand, 2);

    CHECK(clock_preset("clock2", longp, along, dlong, 5, next, &better) == 0);
    CHECK_EQ(better.step[0].victim, 2);
    CHECK_EQ(better.step[0].passes, 2);
    CHECK_EQ(better.step[0].slots[2], 9);
    CHECK_EQ(better.step[0].refbit[0], 0);
    CHECK_EQ(better.step[0].dirty[0], 1);
    CHECK_EQ(better.step[0].refbit[1], 0);
    CHECK_EQ(better.step[0].dirty[1], 0);
    CHECK_EQ(better.step[0].hand, 3);
}

static void test_factor(void) {
    static const int addr[] = {0, 64, 128, 192, 256, 320, 0, 64};
    static const int phase[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2,
                                 3, 4, 5, 3, 4, 5, 3, 4, 5, 3, 4, 5};
    Access refs[8];
    int i;

    for (i = 0; i < 8; i++) {
        refs[i].page = addr[i] / 256;
        refs[i].write = 0;
    }
    {
        Report r;
        CHECK(sim_exec("lru", 2, NULL, 0, refs, 8, &r) == 0);
        CHECK_EQ(r.faults, 2);
        for (i = 0; i < 8; i++) {
            refs[i].page = addr[i] / 64;
        }
        CHECK(sim_exec("lru", 2, NULL, 0, refs, 8, &r) == 0);
        CHECK_EQ(r.faults, 8);
    }
    CHECK_EQ(faults_of("lru", 3, phase, 24), 6);
    CHECK_EQ(faults_of("lru", 2, phase, 24), 24);
}

static void test_parse(void) {
    Access a;
    int page = -1;
    int aref = -1;
    int dirty = -1;

    CHECK(parse_ref("7", &a) == 0 && a.page == 7 && a.write == 0);
    CHECK(parse_ref("7w", &a) == 0 && a.write == 1);
    CHECK(parse_ref("7x", &a) != 0);
    CHECK(parse_cell("2:0.1", &page, &aref, &dirty) == 0);
    CHECK_EQ(page, 2);
    CHECK_EQ(aref, 0);
    CHECK_EQ(dirty, 1);
    CHECK(parse_cell("2:2.0", &page, &aref, &dirty) != 0);
}

static void test_belady(void) {
    static const int classic[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    BeladyFound found;
    int i;

    CHECK(belady_search(&found) == 0);
    CHECK_EQ(found.len, 12);
    for (i = 0; i < 12; i++) {
        CHECK_EQ(found.refs[i], classic[i]);
    }
    CHECK_EQ(found.faults[0][0], 9);
    CHECK_EQ(found.faults[0][1], 10);
    CHECK_EQ(found.faults[1][0], 10);
    CHECK_EQ(found.faults[1][1], 8);
    CHECK_EQ(found.faults[2][0], 7);
    CHECK_EQ(found.faults[2][1], 6);
    CHECK_EQ(found.faults[3][0], 9);
    CHECK_EQ(found.faults[3][1], 10);
    CHECK(found.faults[4][0] > 0);
    CHECK(found.faults[1][0] >= found.faults[1][1]);
    CHECK(found.faults[2][0] >= found.faults[2][1]);
}

int main(void) {
    test_order();
    test_parse();
    test_split();
    test_tang();
    test_lru_exams();
    test_work_exam();
    test_clock2();
    test_factor();
    test_belady();
    return TEST_SUMMARY();
}
