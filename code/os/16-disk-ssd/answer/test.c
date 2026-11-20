#include "ftl.h"
#include "geom.h"
#include "sched.h"
#include "test.h"
#include "time.h"

static void expect_order(const SchedOut *out, const int *want, int n, int move) {
    int i;

    CHECK_EQ(out->n, n);
    CHECK_EQ(out->move, move);
    for (i = 0; i < n; i++) {
        CHECK_EQ(out->track[i], want[i]);
    }
}

static void test_textbook(void) {
    static const int req[] = {98, 183, 37, 122, 14, 124, 65, 67};
    static const int fcfs[] = {98, 183, 37, 122, 14, 124, 65, 67};
    static const int sstf[] = {65, 67, 37, 14, 98, 122, 124, 183};
    static const int scan[] = {65, 67, 98, 122, 124, 183, 37, 14};
    static const int cscan[] = {65, 67, 98, 122, 124, 183, 14, 37};
    SchedOut out;

    CHECK_EQ(sched_run("fcfs", 53, 0, 1, 199, req, 8, &out), 0);
    expect_order(&out, fcfs, 8, 640);
    CHECK_EQ(sched_run("sstf", 53, 0, 1, 199, req, 8, &out), 0);
    expect_order(&out, sstf, 8, 236);
    CHECK_EQ(sched_run("scan", 53, 1, 1, 199, req, 8, &out), 0);
    expect_order(&out, scan, 8, 331);
    CHECK_EQ(sched_run("look", 53, 1, 1, 199, req, 8, &out), 0);
    expect_order(&out, scan, 8, 299);
    CHECK_EQ(sched_run("cscan", 53, 1, 1, 199, req, 8, &out), 0);
    expect_order(&out, cscan, 8, 382);
    CHECK_EQ(sched_run("clook", 53, 1, 0, 0, req, 8, &out), 0);
    expect_order(&out, cscan, 8, 322);
}

static void test_exams(void) {
    static const int y2015[] = {130, 42, 180, 15, 199};
    static const int o2015[] = {130, 180, 199, 42, 15};
    static const int y2021[] = {184, 187, 176, 182, 199};
    static const int o2021[] = {184, 182, 187, 176, 199};
    static const int y2024[] = {300, 120, 110, 0, 160, 210, 399};
    static const int o2024[] = {160, 120, 110, 0, 399, 300, 210};
    static const int y2010[] = {50, 90, 30, 120};
    static const int o2010[] = {120, 30, 50, 90};
    static const int tie[] = {60, 40};
    static const int otie[] = {40, 60};
    SchedOut out;

    CHECK_EQ(sched_run("scan", 58, 1, 1, 199, y2015, 5, &out), 0);
    expect_order(&out, o2015, 5, 325);
    CHECK_EQ(sched_run("sstf", 184, 0, 1, 199, y2021, 5, &out), 0);
    expect_order(&out, o2021, 5, 41);
    CHECK_EQ(sched_run("cscan", 200, -1, 1, 399, y2024, 7, &out), 0);
    expect_order(&out, o2024, 7, 788);
    CHECK_EQ(sched_run("clook", 100, 1, 0, 0, y2010, 4, &out), 0);
    expect_order(&out, o2010, 4, 170);
    CHECK_EQ(sched_run("sstf", 50, 0, 0, 0, tie, 2, &out), 0);
    expect_order(&out, otie, 2, 30);
    CHECK_EQ(sched_run("scan", 10, 0, 1, 100, tie, 2, &out), -1);
    CHECK_EQ(sched_run("cscan", 10, 1, 0, 0, tie, 2, &out), -1);
}

static void test_time(void) {
    TimeParts parts;
    char exact[32];
    char round1[32];
    int ticks;
    int wait;
    int proc;

    CHECK_EQ(time_each(8000, 7200, 1000, 1, 0, &parts), 0);
    fmt_ms(parts.seek + parts.rot + parts.xfer, 3, exact, (int)sizeof exact);
    fmt_ms(parts.seek + parts.rot + parts.xfer, 1, round1, (int)sizeof round1);
    CHECK_STR(exact, "12.175");
    CHECK_STR(round1, "12.2");
    CHECK_EQ(time_rate(6000, 10000, 4000, 20000000, 200, &parts), 0);
    fmt_ms(parts.seek + parts.rot + parts.xfer + parts.ctrl, 1, round1, (int)sizeof round1);
    CHECK_STR(round1, "9.4");
    CHECK_EQ(time_sum(170000, 6000, 100, 4, 0, &parts), 0);
    fmt_ms(parts.seek + parts.rot + parts.xfer, 1, round1, (int)sizeof round1);
    CHECK_STR(round1, "190.4");
    CHECK_EQ(time_each(8000, 7201, 1000, 1, 0, &parts), -1);
    CHECK_EQ(layout_run(7, 0, 4, 1, &ticks, &wait, &proc), 0);
    CHECK_EQ(ticks, 25);
    CHECK_EQ(wait, 18);
    CHECK_EQ(proc, 3);
    CHECK_EQ(layout_run(7, 1, 4, 1, &ticks, &wait, &proc), 0);
    CHECK_EQ(ticks, 7);
    CHECK_EQ(wait, 0);
    CHECK_EQ(layout_run(8, 1, 4, 1, &ticks, &wait, &proc), -1);
}

static void test_geom(void) {
    static const int clusters[] = {100260, 60005, 101660, 110560};
    static const int want[] = {100260, 101660, 110560, 60005};
    int order[4];
    int cyl;
    int head;
    int sector;
    int i;

    CHECK_EQ(disk_bytes(300, 10, 200, 512), 307200000LL);
    CHECK_EQ(cluster_chs(10, 200, 2, 100530, &cyl, &head, &sector), 0);
    CHECK_EQ(cyl, 100);
    CHECK_EQ(head, 5);
    CHECK_EQ(sector, 60);
    CHECK_EQ(cluster_sstf(10, 200, 2, 85, clusters, 4, order), 0);
    for (i = 0; i < 4; i++) {
        CHECK_EQ(order[i], want[i]);
    }
    CHECK_EQ(cluster_chs(10, 200, 3, 10, &cyl, &head, &sector), -1);
}

static void test_ftl(void) {
    int none[FTL_BLKS];
    int dynamic[FTL_BLKS];
    int host;
    int flash;
    Ftl disk;
    int old_invalid = 0;
    int bi;
    int pi;

    CHECK_EQ(ftl_wear(2, 2, 8, 0, none, &host, &flash), 0);
    CHECK_EQ(none[0], 2);
    CHECK_EQ(none[1], 0);
    CHECK_EQ(host, 8);
    CHECK_EQ(ftl_wear(2, 2, 8, 1, dynamic, &host, &flash), 0);
    CHECK_EQ(dynamic[0], 1);
    CHECK_EQ(dynamic[1], 1);
    ftl_init(&disk, 2, 2, 0);
    CHECK_EQ(ftl_write(&disk, 0, 1), 0);
    CHECK_EQ(ftl_write(&disk, 1, 2), 0);
    CHECK_EQ(ftl_write(&disk, 0, 9), 0);
    for (bi = 0; bi < disk.nblk; bi++) {
        for (pi = 0; pi < disk.npg; pi++) {
            if (disk.page[bi][pi].state == 2) {
                old_invalid++;
            }
            if (disk.page[bi][pi].state == 1 && disk.page[bi][pi].lpn == 0) {
                CHECK_EQ(disk.page[bi][pi].value, 9);
            }
        }
    }
    CHECK_EQ(old_invalid, 1);
}

static void test_stick(void) {
    int order[16];
    int n = 0;
    int stuck = 0;

    CHECK_EQ(stick_run("fcfs", 3, order, &n, &stuck), 0);
    CHECK_EQ(stuck, 0);
    CHECK_EQ(order[0], 50);
    CHECK_EQ(order[1], 80);
    CHECK_EQ(stick_run("sstf", 3, order, &n, &stuck), 0);
    CHECK_EQ(stuck, 1);
    CHECK_EQ(order[3], 50);
    CHECK_EQ(order[4], 80);
    CHECK_EQ(stick_run("scan", 3, order, &n, &stuck), 0);
    CHECK_EQ(stuck, 1);
    CHECK_EQ(stick_run("cscan", 3, order, &n, &stuck), 0);
    CHECK_EQ(stuck, 1);
}

int main(void) {
    test_textbook();
    test_exams();
    test_time();
    test_geom();
    test_ftl();
    test_stick();
    return TEST_SUMMARY();
}
