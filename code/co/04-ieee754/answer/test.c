#include "fp.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same_encode(const char *text) {
    Decimal dec;
    EncShow show;
    char err[120];
    if (!dec_parse(text, &dec, err, sizeof err)) {
        fprintf(stderr, "解析失败 %s：%s\n", text, err);
        return 0;
    }
    if (!fp_encode(&dec, &FMT32, &show, err, sizeof err)) {
        fprintf(stderr, "单精度编码失败 %s：%s\n", text, err);
        return 0;
    }
    if (show.bits != c_float_bits(text)) {
        fprintf(stderr, "单精度不一致 %s：手算 %llx，C %x\n", text,
                (unsigned long long)show.bits, c_float_bits(text));
        return 0;
    }
    if (!fp_encode(&dec, &FMT64, &show, err, sizeof err)) {
        fprintf(stderr, "双精度编码失败 %s：%s\n", text, err);
        return 0;
    }
    if (show.bits != c_double_bits(text)) {
        fprintf(stderr, "双精度不一致 %s：手算 %llx，C %llx\n", text,
                (unsigned long long)show.bits, (unsigned long long)c_double_bits(text));
        return 0;
    }
    return 1;
}

static void test_encode_samples(void) {
    static const char *samples[] = {
        "0",     "-0",    "0.0",   "-0.0",  "1",     "-1",    "2",     "0.5",
        "-8.25", "8.25",  "0.1",   "0.2",   "0.3",   "12.1",  "1.5",   "7.25",
        "-33",   "16777216", "16777217", "16777218", "16777219", "16777220",
        "16777221", "1e20", "1e39", "1e-45", "-0.75", "0.75", "123.456",
        "2.5e+2", "6.02e23", "2147483647", "2147483648", "-2147483648",
        "0.0000001", "45056", "12288", "1.1754944e-38", "3.4028235e38",
    };
    int i;
    int bad = 0;
    for (i = 0; i < (int)(sizeof samples / sizeof samples[0]); i++) {
        if (!same_encode(samples[i])) {
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

static void test_random_decimal(void) {
    int i;
    int bad = 0;
    uint32_t state = 1;
    for (i = 0; i < 400; i++) {
        char buf[32];
        int coef;
        int exp10;
        state = state * 1664525u + 1013904223u;
        coef = (int)(state % 2000000) - 1000000;
        state = state * 1664525u + 1013904223u;
        exp10 = (int)(state % 31) - 15;
        snprintf(buf, sizeof buf, "%de%d", coef, exp10);
        if (!same_encode(buf)) {
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

static int nan_bits(uint32_t bits) {
    return fp_view(bits, &FMT32).kind == KIND_NAN;
}

static int same_add(uint32_t a, uint32_t b) {
    AddInfo info;
    uint32_t hardware;
    fadd_info(a, b, &info);
    hardware = c_fadd(a, b);
    if (nan_bits(a) || nan_bits(b) || nan_bits(info.z_bits) || nan_bits(hardware)) {
        return nan_bits(info.z_bits) && nan_bits(hardware);
    }
    if (info.z_bits != hardware) {
        fprintf(stderr, "加法不一致 %08x + %08x：手算 %08x，C %08x\n", a, b, info.z_bits,
                hardware);
        return 0;
    }
    return info.c_match == 1;
}

static void test_add_pairs(void) {
    static const uint32_t bits[] = {
        0x00000000u, 0x80000000u, 0x00000001u, 0x007fffffu, 0x80000001u, 0x00800000u,
        0x3f800000u, 0x3f000000u, 0x40000000u, 0xbf800000u, 0x7f7fffffu, 0xff7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc00000u, 0x00800001u, 0x3f800001u, 0x4b800000u,
        0x33800000u, 0x40e80000u, 0xc2040000u, 0x3fc00000u,
    };
    int n = (int)(sizeof bits / sizeof bits[0]);
    int i;
    int j;
    int bad = 0;
    uint32_t state = 7;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (!same_add(bits[i], bits[j])) {
                bad++;
            }
        }
    }
    for (i = 0; i < 800; i++) {
        state = state * 1664525u + 1013904223u;
        if (!same_add(state, state * 17u + 3u)) {
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

static void test_exam_values(void) {
    char buf[64];
    View v;
    v = fp_view(0xC6400000u, &FMT32);
    fp_exam(&v, &FMT32, buf, sizeof buf);
    CHECK_STR(buf, "-1.5×2^13");
    v = fp_view(0x80200000u, &FMT32);
    fp_exam(&v, &FMT32, buf, sizeof buf);
    CHECK_STR(buf, "-2^(-128)");
    v = fp_view(0x47300000u, &FMT32);
    fp_exam(&v, &FMT32, buf, sizeof buf);
    CHECK_STR(buf, "+1.375×2^15");
    v = fp_view(0x00800000u, &FMT32);
    fp_exam(&v, &FMT32, buf, sizeof buf);
    CHECK_STR(buf, "+1.0×2^(-126)");
    CHECK(same_encode("-8.25"));
    CHECK(c_float_bits("-8.25") == 0xC1040000u);
    CHECK(c_float_bits("12.1") == 0x4141999Au);
}

int main(void) {
    test_encode_samples();
    test_random_decimal();
    test_add_pairs();
    test_exam_values();
    return TEST_SUMMARY();
}
