#include <math.h>

#include "format.h"
#include "perf.h"
#include "test.h"

/* 真题的数都落在整数刻度上，用 llround 比较，避免直接比 double */

static void test_parse_units(void) {
    double hz = 0;
    double seconds = 0;
    double speedup = 0;
    double percent = 0;
    bool from_percent = false;
    int exp10 = 0;
    CHECK(parse_hz(1.2, "GHz", &hz));
    CHECK_EQ(llround(hz), 1200000000LL);
    CHECK(parse_hz(1, "GHz", &hz));
    CHECK_EQ(llround(hz), 1000000000LL);
    CHECK(parse_seconds(28, "us", &seconds));
    CHECK_EQ(llround(seconds * 1e6), 28);
    CHECK(parse_speedup("+50%", &speedup, &from_percent, &percent));
    CHECK(from_percent);
    CHECK_EQ(llround(percent), 50);
    CHECK_EQ(llround(speedup * 10), 15);
    CHECK(parse_flops_unit("GFLOPS", &exp10));
    CHECK_EQ(exp10, 9);
    CHECK(!parse_hz(1, "MB", &hz));
}

static void test_2013_mips(void) {
    double ratio[] = {0.5, 0.2, 0.1, 0.2};
    double cpi[] = {2, 3, 4, 5};
    double hz = 0;
    double avg = weighted_cpi(ratio, cpi, 4);
    double wrong;
    CHECK(parse_hz(1.2, "GHz", &hz));
    CHECK_EQ(llround(avg * 10), 30);
    CHECK_EQ(llround(mips_of(hz, avg)), 400);
    /* 把 M 当成 2^20 就不是 400，这是性能单位和存储单位的分界 */
    wrong = hz / (avg * 1048576.0);
    CHECK(llround(wrong) != 400);
}

static void test_2022_time(void) {
    double ratio[] = {0.8, 0.2};
    double cpi[] = {1, 10};
    double hz = 0;
    double avg = weighted_cpi(ratio, cpi, 2);
    double seconds;
    CHECK(parse_hz(1, "GHz", &hz));
    CHECK_EQ(llround(avg * 10), 28);
    seconds = cpu_seconds(10000, avg, hz);
    CHECK_EQ(llround(seconds * 1e6), 28);
}

static void test_2023_gips(void) {
    double hz = 0;
    double seconds;
    CHECK(parse_hz(1.5, "GHz", &hz));
    seconds = cpu_seconds(500000, 1.2, hz);
    CHECK_EQ(llround(seconds * 1e6), 400);
    CHECK_EQ(llround(gips_of(hz, 1.2) * 100), 125);
    CHECK_EQ(llround(mips_of(hz, 1.2)), 1250);
}

static void test_2014_scale(void) {
    double neu = scale_time(20, 0.7, 1.2);
    CHECK_EQ(llround(neu * 10), 168);
    CHECK(llround(speedup_of(20, neu) * 10) > 10);
}

static void test_2012_amdahl(void) {
    double neu = amdahl_time(100, 90, 1.5);
    CHECK_EQ(llround(neu), 70);
    CHECK(llround(amdahl_time(100, 90, 2)) == 55);
}

static void test_2017_ratio(void) {
    double hz1 = 0;
    double hz2 = 0;
    CHECK(parse_hz(1.5, "GHz", &hz1));
    CHECK(parse_hz(1.2, "GHz", &hz2));
    CHECK_EQ(llround(time_ratio(2, hz1, 1, hz2) * 10), 16);
}

static void test_flops(void) {
    CHECK_EQ(llround(flops_convert(3.6, 9, 6)), 3600);
    CHECK_EQ(llround(mflops_of(20000000, 0.5)), 40);
    CHECK_EQ(llround(flops_convert(40, 6, 9) * 100), 4);
}

int main(void) {
    test_parse_units();
    test_2013_mips();
    test_2022_time();
    test_2023_gips();
    test_2014_scale();
    test_2012_amdahl();
    test_2017_ratio();
    test_flops();
    return TEST_SUMMARY();
}
