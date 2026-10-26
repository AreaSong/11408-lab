#include "amdahl.h"
#include "exam.h"
#include "flynn.h"
#include "line.h"
#include "simd.h"
#include "test.h"

static void test_amdahl(void) {
    Amdahl a = amdahl_of(80, 4);
    CHECK_EQ(a.speed_num, 5);
    CHECK_EQ(a.speed_den, 2);
    CHECK_EQ(a.eff_num, 5);
    CHECK_EQ(a.eff_den, 8);
    CHECK_EQ(a.limit_infinite, 0);
    CHECK_EQ(a.limit_num, 5);
    CHECK_EQ(a.limit_den, 1);

    a = amdahl_of(80, 1);
    CHECK_EQ(a.speed_num, 1);
    CHECK_EQ(a.speed_den, 1);

    a = amdahl_of(80, 2);
    CHECK_EQ(a.speed_num, 5);
    CHECK_EQ(a.speed_den, 3);

    a = amdahl_of(80, 8);
    CHECK_EQ(a.speed_num, 10);
    CHECK_EQ(a.speed_den, 3);
    CHECK_EQ(a.eff_num, 5);
    CHECK_EQ(a.eff_den, 12);

    a = amdahl_of(100, 8);
    CHECK_EQ(a.speed_num, 8);
    CHECK_EQ(a.speed_den, 1);
    CHECK_EQ(a.eff_num, 1);
    CHECK(a.limit_infinite);

    a = amdahl_of(0, 4);
    CHECK_EQ(a.speed_num, 1);
    CHECK_EQ(a.speed_den, 1);
    CHECK_EQ(a.eff_num, 1);
    CHECK_EQ(a.eff_den, 4);
    CHECK_EQ(a.limit_num, 1);

    a = amdahl_of(90, 8);
    CHECK_EQ(a.speed_num, 80);
    CHECK_EQ(a.speed_den, 17);
    CHECK_EQ(a.eff_num, 10);
    CHECK_EQ(a.eff_den, 17);
    CHECK_EQ(a.limit_num, 10);
}

static void test_line(void) {
    CHECK_EQ(line_kind(128, 0, 4), LINE_FALSE);
    CHECK_EQ(line_kind(128, 0, 128), LINE_NONE);
    CHECK_EQ(line_kind(128, 0, 0), LINE_TRUE);
    CHECK_EQ(line_kind(128, 0x100, 0x104), LINE_FALSE);
    CHECK_EQ(line_kind(128, 0x100, 0x180), LINE_NONE);
    CHECK_EQ(line_kind(64, 0, 60), LINE_FALSE);
    CHECK_EQ(line_kind(64, 0, 64), LINE_NONE);
}

static void test_simd(void) {
    SimdInfo s = simd_of(128, 32, 1000);
    CHECK(s.ok);
    CHECK_EQ(s.lanes, 4);
    CHECK_EQ(s.vector_insns, 250);
    CHECK_EQ(s.speed_num, 4);
    CHECK_EQ(s.speed_den, 1);

    s = simd_of(128, 32, 6);
    CHECK_EQ(s.vector_insns, 2);
    CHECK_EQ(s.speed_num, 3);
    CHECK_EQ(s.speed_den, 1);

    s = simd_of(128, 32, 5);
    CHECK_EQ(s.vector_insns, 2);
    CHECK_EQ(s.speed_num, 5);
    CHECK_EQ(s.speed_den, 2);

    s = simd_of(128, 24, 8);
    CHECK_EQ(s.ok, 0);
}

static void test_names(void) {
    CHECK_STR(flynn_step("sisd"), "1");
    CHECK_STR(flynn_step("SIMD"), "4");
    CHECK_STR(flynn_step("misd"), "无");
    CHECK_STR(flynn_step("Mimd"), "2");
    CHECK(flynn_step("gpu") == NULL);
    CHECK_STR(exam_letter("2022-22"), "C");
    CHECK(exam_letter("2017-17") == NULL);
}

int main(void) {
    test_amdahl();
    test_line();
    test_simd();
    test_names();
    return TEST_SUMMARY();
}
