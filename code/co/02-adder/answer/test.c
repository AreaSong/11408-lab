#include "alu.h"
#include "cla.h"
#include "mix.h"
#include "shift.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static Word must_parse(int width, const char *text) {
    Word out;
    char err[80];
    CHECK(word_parse(text, width, &out, err, sizeof err));
    return out;
}

static void test_full_adder(void) {
    static const int row[][5] = {
        {0, 0, 0, 0, 0}, {0, 0, 1, 1, 0}, {0, 1, 0, 1, 0}, {0, 1, 1, 0, 1},
        {1, 0, 0, 1, 0}, {1, 0, 1, 0, 1}, {1, 1, 0, 0, 1}, {1, 1, 1, 1, 1},
    };
    int i;
    for (i = 0; i < 8; i++) {
        FullAdd bit = full_add(row[i][0], row[i][1], row[i][2]);
        CHECK_EQ(bit.sum, row[i][3]);
        CHECK_EQ(bit.cout, row[i][4]);
    }
}

static int pair_ok(Word a, Word b, int sub) {
    AluResult got = alu_exec(sub ? ALU_SUB : ALU_ADD, a, b);
    long long math = sub ? word_signed(a) - word_signed(b) : word_signed(a) + word_signed(b);
    unsigned long long ua = word_unsigned(a);
    unsigned long long ub = word_unsigned(b);
    int of = math > word_smax(a.width) || math < word_smin(a.width);
    int cf = sub ? ua < ub : (ua + ub) > word_mask(a.width);
    if (got.math_signed != math || got.of != of || got.cf != cf) {
        return 0;
    }
    if (got.of != (got.sign_hi != got.sign_lo)) {
        return 0;
    }
    if ((got.wide.bits & word_mask(a.width)) != got.result.bits) {
        return 0;
    }
    if (got.zf != (got.result.bits == 0) || got.sf != word_bit(got.result, a.width - 1)) {
        return 0;
    }
    return 1;
}

static void test_eight_bit_space(void) {
    int a;
    int b;
    int bad = 0;
    for (a = 0; a < 256; a++) {
        for (b = 0; b < 256; b++) {
            Word wa = word_make(8, (unsigned)a);
            Word wb = word_make(8, (unsigned)b);
            if (!pair_ok(wa, wb, 0) || !pair_ok(wa, wb, 1)) {
                bad++;
            }
        }
    }
    CHECK_EQ(bad, 0);
}

static void test_cla_matches_ripple(void) {
    int a;
    int b;
    int cin;
    int bad = 0;
    for (a = 0; a < 16; a++) {
        for (b = 0; b < 16; b++) {
            for (cin = 0; cin < 2; cin++) {
                Word wa = word_make(4, (unsigned)a);
                Word wb = word_make(4, (unsigned)b);
                Cla4 fast = cla4(wa, wb, cin);
                Ripple slow = ripple_add(wa, wb, cin);
                if (fast.sum != slow.sum || fast.c[4] != slow.carry[4]) {
                    bad++;
                }
            }
        }
    }
    CHECK_EQ(bad, 0);
}

static void expect_flags(AluOp op, int width, const char *left, const char *right, unsigned long long bits, int cf,
                         int of) {
    AluResult got = alu_exec(op, must_parse(width, left), must_parse(width, right));
    CHECK_EQ(got.result.bits, bits);
    CHECK_EQ(got.cf, cf);
    CHECK_EQ(got.of, of);
}

static void test_exam_flags(void) {
    AluResult add;
    AluResult sub;
    expect_flags(ALU_SUB, 8, "134u", "246u", 0x90, 1, 0);
    expect_flags(ALU_ADD, 8, "134u", "246u", 0x7C, 1, 1);
    sub = alu_exec(ALU_SUB, must_parse(8, "134u"), must_parse(8, "246u"));
    CHECK_EQ(word_signed(must_parse(8, "86H")), -122);
    CHECK_EQ(word_signed(sub.result), -112);
    add = alu_exec(ALU_ADD, must_parse(8, "134u"), must_parse(8, "246u"));
    CHECK_EQ(add.math_signed, -132);
    CHECK_EQ(add.sign_hi, 1);
    CHECK_EQ(add.sign_lo, 0);
    expect_flags(ALU_SUB, 8, "A3H", "75H", 0x2E, 0, 1);
    expect_flags(ALU_SUB, 32, "100", "200", 0xFFFFFF9CULL, 1, 0);
    expect_flags(ALU_SUB, 32, "10", "-20", 0x1E, 1, 0);
    expect_flags(ALU_ADD, 32, "87654321H", "98765432H", 0x1FDB9753ULL, 1, 1);
    expect_flags(ALU_SUB, 32, "0u", "FFFFFFFFH", 0x1, 1, 0);
    sub = alu_exec(ALU_ADD, must_parse(8, "1u"), must_parse(8, "FFH"));
    CHECK_EQ(sub.zf, 1);
    CHECK_EQ(sub.cf, 1);
    CHECK_EQ(sub.of, 0);
}

static void test_shift(void) {
    ShiftResult arith = shift_apply(must_parse(16, "-513"), 1, 1, 1);
    ShiftResult logic = shift_apply(must_parse(8, "0b11011000"), 0, 1, 1);
    ShiftResult sign = shift_apply(must_parse(8, "0b11011000"), 1, 1, 1);
    ShiftResult left = shift_apply(must_parse(8, "64"), 1, 0, 1);
    CHECK_EQ(must_parse(16, "-513").bits, 0xFDFF);
    CHECK_EQ(arith.result.bits, 0xFEFF);
    CHECK_EQ(word_signed(arith.result), -257);
    CHECK_EQ(logic.result.bits, 0x6C);
    CHECK_EQ(sign.result.bits, 0xEC);
    CHECK_EQ(left.result.bits, 0x80);
    CHECK_EQ(left.overflow, 1);
    CHECK_EQ(word_signed(left.result), -128);
}

static void test_mix(void) {
    MixResult cmp_rank;
    MixResult cmp_short;
    MixResult cmp_wide;
    MixResult sub;
    WrapInfo dead;
    WrapInfo live;
    FitInfo overflow;
    FitInfo edge;
    char err[80];
    CHECK(mix_eval(MIX_CMP, TY_INT, "-1", TY_UINT, "1", &cmp_rank, err, sizeof err));
    CHECK_EQ(cmp_rank.rel, 1);
    CHECK_EQ(cmp_rank.left_common.uval, 4294967295ULL);
    CHECK(mix_eval(MIX_CMP, TY_USHORT, "65535", TY_INT, "-1", &cmp_short, err, sizeof err));
    CHECK_EQ(cmp_short.common, TY_INT);
    CHECK_EQ(cmp_short.rel, 1);
    CHECK_EQ(cmp_short.left_promoted.bits, 0xFFFFULL);
    CHECK(mix_eval(MIX_CMP, TY_UINT, "65535", TY_INT, "-1", &cmp_wide, err, sizeof err));
    CHECK_EQ(cmp_wide.common, TY_UINT);
    CHECK_EQ(cmp_wide.rel, -1);
    CHECK(mix_eval(MIX_SUB, TY_INT, "-1", TY_UINT, "1", &sub, err, sizeof err));
    CHECK_EQ(sub.alu.result.bits, 0xFFFFFFFEULL);
    CHECK(wrap_eval(1, 0, &dead, err, sizeof err));
    CHECK_EQ(dead.infinite, 1);
    CHECK_EQ(dead.after.bits, 0xFFFFFFFFULL);
    CHECK(wrap_eval(0, 0, &live, err, sizeof err));
    CHECK_EQ(live.infinite, 0);
    CHECK_EQ(live.entered, 0);
    CHECK_EQ(word_signed(live.after), -1);
    CHECK(fit_eval(0, 4294967295LL, &overflow, err, sizeof err));
    CHECK_EQ(overflow.fits, 0);
    CHECK_EQ(word_signed(overflow.stored), -1);
    CHECK(fit_eval(0, 2147483647LL, &edge, err, sizeof err));
    CHECK_EQ(edge.fits, 1);
}

static void test_logic(void) {
    AluResult band = alu_exec(ALU_AND, must_parse(8, "F0H"), must_parse(8, "3CH"));
    AluResult bor = alu_exec(ALU_OR, must_parse(8, "F0H"), must_parse(8, "0FH"));
    CHECK_EQ(band.has_flags, 0);
    CHECK_EQ(band.result.bits, 0x30);
    CHECK_EQ(bor.result.bits, 0xFF);
}

int main(void) {
    test_full_adder();
    test_eight_bit_space();
    test_cla_matches_ripple();
    test_exam_flags();
    test_shift();
    test_mix();
    test_logic();
    return TEST_SUMMARY();
}
