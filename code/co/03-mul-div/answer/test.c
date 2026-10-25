#include "booth.h"
#include "divide.h"
#include "exam.h"
#include "orig.h"
#include "test.h"
#include "word.h"

#include <stdio.h>

static Frac must_frac(const char *text) {
    Frac out = {0, 0, 0};
    char err[80];
    CHECK(frac_parse(text, &out, err, sizeof err));
    return out;
}

static Word must_word(int width, const char *text) {
    Word out = word_make(width, 0);
    char err[80];
    CHECK(word_parse(text, width, &out, err, sizeof err));
    return out;
}

static void test_orig_space(void) {
    int n;
    int bad = 0;
    for (n = 1; n <= 5; n++) {
        int lim = 1 << n;
        int sx;
        int mx;
        int sy;
        int my;
        for (sx = 0; sx < 2; sx++) {
            for (mx = 0; mx < lim; mx++) {
                for (sy = 0; sy < 2; sy++) {
                    for (my = 0; my < lim; my++) {
                        Frac x = {n, sx, (unsigned)mx};
                        Frac y = {n, sy, (unsigned)my};
                        Orig o;
                        char err[40];
                        if (!orig_eval(x, y, &o, err, sizeof err) || o.sign != (sx ^ sy) ||
                            o.mag != (unsigned)(mx * my)) {
                            bad++;
                        }
                    }
                }
            }
        }
    }
    CHECK_EQ(bad, 0);
}

static void test_booth_space(void) {
    int n;
    int bad = 0;
    for (n = 1; n <= 4; n++) {
        int lim = 1 << n;
        int sx;
        int mx;
        int sy;
        int my;
        for (sx = 0; sx < 2; sx++) {
            for (mx = 0; mx < lim; mx++) {
                for (sy = 0; sy < 2; sy++) {
                    for (my = 0; my < lim; my++) {
                        Frac x = {n, sx, (unsigned)mx};
                        Frac y = {n, sy, (unsigned)my};
                        Booth o;
                        char err[40];
                        long long expect = frac_num(x) * frac_num(y);
                        if (!booth_eval(x, y, &o, err, sizeof err) || !o.signs_ok || o.value != expect) {
                            bad++;
                        }
                    }
                }
            }
        }
    }
    CHECK_EQ(bad, 0);
}

static int div_bad(Frac x, Frac y) {
    Divide o;
    char err[40];
    long long xv;
    long long yv;
    long long qv;
    if (!divide_eval(x, y, &o, err, sizeof err)) {
        return 1;
    }
    if (y.mag == 0) {
        return o.kind != 2;
    }
    if (x.mag >= y.mag) {
        return o.kind != 1;
    }
    if (o.kind != 0 || (o.machine_rem >> x.n) != 0) {
        return 1;
    }
    xv = frac_num(x);
    yv = frac_num(y);
    qv = o.qsign ? -(long long)o.q : (long long)o.q;
    return qv * yv + o.rem_num != (xv << x.n);
}

static void test_div_space(void) {
    int n;
    int bad = 0;
    for (n = 1; n <= 4; n++) {
        int lim = 1 << n;
        int sx;
        int mx;
        int sy;
        int my;
        for (sx = 0; sx < 2; sx++) {
            for (mx = 0; mx < lim; mx++) {
                for (sy = 0; sy < 2; sy++) {
                    for (my = 0; my < lim; my++) {
                        Frac x = {n, sx, (unsigned)mx};
                        Frac y = {n, sy, (unsigned)my};
                        bad += div_bad(x, y);
                    }
                }
            }
        }
    }
    CHECK_EQ(bad, 0);
}

static void test_classic_steps(void) {
    Orig o;
    Booth b;
    Divide d;
    Divide flip;
    char err[80];
    CHECK(orig_eval(must_frac("-0.1101"), must_frac("+0.1011"), &o, err, sizeof err));
    CHECK_EQ(o.step[1].part, 0x13);
    CHECK_EQ(o.mag, 0x8F);
    CHECK_EQ(o.sign, 1);
    CHECK(booth_eval(must_frac("-0.1101"), must_frac("+0.1011"), &b, err, sizeof err));
    CHECK_EQ(b.step[0].op, -1);
    CHECK_EQ(b.step[4].shifted, 0);
    CHECK_EQ(b.product, 0x171);
    CHECK_EQ(b.value, -143);
    CHECK(divide_eval(must_frac("+0.1001"), must_frac("+0.1101"), &d, err, sizeof err));
    CHECK_EQ(d.trial, 0x1C);
    CHECK_EQ(d.q, 0xB);
    CHECK_EQ(d.machine_rem, 1);
    CHECK_EQ(d.corrected, 0);
    CHECK_EQ(d.rem_num, 1);
    CHECK(divide_eval(must_frac("+0.0011"), must_frac("+0.1101"), &flip, err, sizeof err));
    CHECK_EQ(flip.step[3].sign_flipped, 1);
    CHECK_EQ(flip.step[3].prev_neg, 0);
    CHECK_EQ(flip.step[3].add, 0);
}

static void test_exam_numbers(void) {
    Word x = must_word(32, "7FFFFFFFH");
    Word y = must_word(32, "2");
    Word neg = must_word(8, "FFH");
    Word two = must_word(8, "2");
    Word dividend = must_word(32, "87654321H");
    Word divisor = must_word(32, "FFH");
    Prod p;
    Prod q;
    prod_eval(x, y, &p);
    CHECK_EQ(p.uprod, 0xFFFFFFFEULL);
    CHECK_EQ(p.sprod, 0xFFFFFFFEULL);
    CHECK_EQ(p.u_over, 0);
    CHECK_EQ(p.s_over, 1);
    prod_eval(neg, two, &q);
    CHECK_EQ(q.uprod, 0x1FE);
    CHECK_EQ(q.sprod, 0xFFFE);
    CHECK_EQ(q.u_over, 1);
    CHECK_EQ(q.s_over, 0);
    CHECK_EQ(cdiv_r(dividend), 0xFFFFFFFFULL);
    CHECK_EQ(cdiv_q(dividend), 0x87654321ULL);
    CHECK_EQ(cdiv_y(divisor), 0xFFULL);
    CHECK_EQ(cdiv_r(must_word(32, "1")), 0);
}

int main(void) {
    test_orig_space();
    test_booth_space();
    test_div_space();
    test_classic_steps();
    test_exam_numbers();
    return TEST_SUMMARY();
}
