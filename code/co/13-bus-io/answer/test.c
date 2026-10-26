#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "io.h"
#include "nest.h"
#include "show.h"
#include "test.h"

static void join_order(const char names[][16], int n, char *buf, size_t buflen) {
    int i;
    size_t used = 0;
    buf[0] = '\0';
    for (i = 0; i < n && used + 1 < buflen; i++) {
        int wrote = snprintf(buf + used, buflen - used, "%s%s", i ? " " : "", names[i]);
        if (wrote > 0) {
            used += (size_t)wrote;
        }
    }
}

static void test_units(void) {
    double hz = 0;
    double seconds = 0;
    double rate = 0;
    CHECK(parse_hz(2.4, "GHz", &hz));
    CHECK_EQ(llround(hz), 2400000000LL);
    CHECK(parse_hz(66, "MHz", &hz));
    CHECK_EQ(llround(hz), 66000000LL);
    CHECK(parse_seconds(0.5, "ms", &seconds));
    CHECK_EQ(llround(seconds * 1e9), 500000LL);
    CHECK(parse_rate(0.5, "MB/s", &rate));
    CHECK_EQ(llround(rate), 500000LL);
    CHECK(!parse_rate(1, "MiB/s", &rate));
}

static void test_bandwidth(void) {
    CHECK_EQ(llround(bw_bps(10e6, 2, 1, 32, 1, 1)), 20000000LL);
    CHECK_EQ(llround(bw_bps(66e6, 1, 2, 32, 1, 1)), 528000000LL);
    CHECK_EQ(llround(bw_bps(2400000000.0, 1, 2, 16, 2, 1) / 1e8), 192LL);
    CHECK_EQ(llround(bw_bps(420e6, 1, 2, 64, 1, 1) / 1e6), 6720LL);
    CHECK_EQ(llround(bw_bps(1333e6, 1, 1, 64, 1, 1) / 1e6), 10664LL);
    CHECK_EQ(llround(bw_bps(1333e6, 1, 1, 64, 1, 3) / 1e6), 31992LL);
    /* 把工作频率再乘 quadpumped 的 4，就是 2025 年的干扰项 */
    CHECK_EQ(llround(bw_bps(1333e6, 1, 4, 64, 1, 1) / 1e6), 42656LL);
}

static void test_txn(void) {
    double burst = txn_seconds(0, 100e6, 1, 1, 0, 4, 0);
    double block = txn_seconds(1, 1e9, 4, 1, 0, 1, 6e-9);
    double y2024 = txn_seconds(0, 420e6, 1, 1, 2, 4, 0);
    CHECK_EQ(llround(burst * 1e9), 50LL);
    CHECK_EQ(llround(block * 1e9), 32LL);
    CHECK_EQ(llround(y2024 * 1e12), 16667LL);
    CHECK_EQ(llround(16.0 / burst), 320000000LL);
    CHECK_EQ(llround(64.0 / y2024 / 1e9 * 100), 384LL);
}

static void test_share(void) {
    double intr = share_ratio(500e6, times_from_rate(0.5e6, 4), 20 * 5);
    double dma = share_ratio(500e6, times_from_rate(5e6, 5000), 500);
    double poll = share_ratio(50e6, 200, 500);
    double query = share_ratio(500e6, times_from_rate(2e6, 4), 10 * 4);
    double disk = share_ratio(500e6, times_from_rate(40e6, 1000), 500);
    CHECK_EQ(llround(intr * 1000), 25LL);
    CHECK_EQ(llround(dma * 1000), 1LL);
    CHECK_EQ(llround(poll * 1000), 2LL);
    CHECK_EQ(llround(query * 1000), 40LL);
    CHECK_EQ(llround(disk * 1000), 40LL);
    CHECK(!keeps_up(4.0 / 40e6, 400.0 / 500e6));
    CHECK(keeps_up(4.0 / 2e6, 40.0 / 500e6));
}

static void test_task_and_steal(void) {
    TaskClocks t = task_clocks(50e6, 0.5e-3, 4, 10, 20, 15, 1000);
    CHECK_EQ(llround(t.total_cycles), 25070000LL);
    CHECK_EQ(llround(t.cpu_cycles), 90000LL);
    CHECK_EQ(llround(t.extra_last), 20LL);
    CHECK_EQ(dma_requests(512, 64), 64LL);
    CHECK_EQ(dma_requests(512, 7), -1LL);
}

static void test_mask_2011(void) {
    /* 位序 L4 L3 L2 L1 L0，处理优先级 L4 > L0 > L2 > L1 > L3 */
    int rank[] = {0, 4, 2, 3, 1};
    char text[8];
    mask_text(mask_for(rank, 5, 0), 5, text, sizeof text);
    CHECK_STR(text, "11111");
    mask_text(mask_for(rank, 5, 4), 5, text, sizeof text);
    CHECK_STR(text, "01111");
    mask_text(mask_for(rank, 5, 2), 5, text, sizeof text);
    CHECK_STR(text, "01110");
    mask_text(mask_for(rank, 5, 3), 5, text, sizeof text);
    CHECK_STR(text, "01010");
    mask_text(mask_for(rank, 5, 1), 5, text, sizeof text);
    CHECK_STR(text, "01000");
}

static void test_nest_orders(void) {
    NestSrc changed[4] = {
        {"A", 0, 0}, {"B", 1, 0}, {"C", 2, 0}, {"D", 3, 0},
    };
    NestSrc same[4] = {
        {"A", 0, 0}, {"B", 1, 0}, {"C", 2, 0}, {"D", 3, 0},
    };
    NestEv all[4] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}};
    NestEv stagger[4] = {{0, 3}, {3, 1}, {4, 0}, {12, 2}};
    char names[8][16];
    char joined[64];
    int n = 0;
    /* 字符串左边是位 0。1111=0xF，0100=0x2，0110=0x6，0111=0xE，0011=0xC，0001=0x8 */
    unsigned masks_changed[] = {0xfu, 0x2u, 0x6u, 0xeu};
    unsigned masks_same[] = {0xfu, 0xeu, 0xcu, 0x8u};
    int i;
    /* 屏蔽字位 0 在字符串左边，数值上是 1<<0，和 mask_text 一致 */
    for (i = 0; i < 4; i++) {
        changed[i].mask = masks_changed[i];
        same[i].mask = masks_same[i];
    }
    CHECK(nest_run(changed, 4, 20, all, 4, names, &n, NULL) == 0);
    join_order(names, n, joined, sizeof joined);
    CHECK_STR(joined, "A D C B");
    CHECK(nest_run(same, 4, 10, stagger, 4, names, &n, NULL) == 0);
    join_order(names, n, joined, sizeof joined);
    CHECK_STR(joined, "A B C D");
}

int main(void) {
    test_units();
    test_bandwidth();
    test_txn();
    test_share();
    test_task_and_steal();
    test_mask_2011();
    test_nest_orders();
    return TEST_SUMMARY();
}
