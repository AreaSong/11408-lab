#include "exam.h"
#include "insn.h"
#include "metric.h"
#include "pipe.h"
#include "test.h"

static void test_metric(void) {
    long long sn;
    long long sd;
    long long en;
    long long ed;
    long long slow[] = {90, 80, 70, 60};
    long long latch[] = {80, 50, 50, 70, 50};
    metric_ratio(5, 100, &sn, &sd, &en, &ed);
    CHECK_EQ(metric_cycles(5, 4), 8);
    CHECK_EQ(metric_cycles(4, 100), 103);
    CHECK_EQ(sn, 125);
    CHECK_EQ(sd, 26);
    CHECK_EQ(en, 25);
    CHECK_EQ(ed, 26);
    CHECK_EQ(metric_period(slow, 4, 0), 90);
    CHECK_EQ(metric_period(latch, 5, 20), 100);
    CHECK_EQ(metric_tp(4, 100, 1030), 1000000000LL);
    CHECK_EQ(exam_letter("2009-18"), 'A');
    CHECK_EQ(exam_letter("2010-19"), 'A');
    CHECK_EQ(exam_letter("2011-18"), 'D');
    CHECK_EQ(exam_letter("2013-18"), 'C');
    CHECK_EQ(exam_letter("2014-16"), 'D');
    CHECK_EQ(exam_letter("2016-19"), 'B');
    CHECK_EQ(exam_letter("2017-17"), 'C');
    CHECK_EQ(exam_letter("2018-20"), 'D');
    CHECK_EQ(exam_letter("2019-18"), 'C');
    CHECK_EQ(exam_letter("2020-17"), 'B');
    CHECK_EQ(exam_letter("2023-19"), 'C');
    CHECK_EQ(exam_letter("2024-19"), 'C');
    CHECK_EQ(exam_letter("2012-44"), 0);
}

static int load_sim(const char *key, PipeStat *st) {
    int rc = exam_sim(key, st);
    CHECK_EQ(rc, 0);
    return rc;
}

static void test_late(void) {
    PipeStat ab;
    PipeStat mul;
    if (load_sim("2012-ab", &ab) != 0 || load_sim("2012-mul", &mul) != 0) {
        return;
    }
    CHECK_EQ(ab.cycles, 14);
    CHECK_EQ(ab.extra, 6);
    CHECK_EQ(ab.id_extra[2], 3);
    CHECK_EQ(ab.id_extra[3], 3);
    CHECK_EQ(mul.cycles, 17);
    CHECK_EQ(mul.extra, 8);
    CHECK_EQ(mul.id_extra[0], 0);
    CHECK_EQ(mul.id_extra[2], 2);
    CHECK_EQ(mul.id_extra[3], 3);
    CHECK_EQ(mul.id_extra[4], 3);
}

static void test_forward_and_branch(void) {
    PipeStat half;
    PipeStat fwd;
    PipeStat use;
    PipeStat y2016;
    PipeStat y2019;
    PipeStat y2023;
    PipeStat y2014;
    if (load_sim("raw-half", &half) != 0 || load_sim("raw-forward", &fwd) != 0 ||
        load_sim("load-use", &use) != 0 || load_sim("2016", &y2016) != 0 ||
        load_sim("2019", &y2019) != 0 || load_sim("2023", &y2023) != 0 ||
        load_sim("2014", &y2014) != 0) {
        return;
    }
    CHECK_EQ(half.extra, 2);
    CHECK_EQ(half.id_extra[1], 2);
    CHECK_EQ(half.id_extra[2], 0);
    CHECK_EQ(fwd.extra, 0);
    CHECK_EQ(fwd.cycles, 8);
    CHECK_EQ(use.extra, 1);
    CHECK_EQ(use.cycles, 7);
    CHECK_EQ(use.id_extra[1], 1);
    CHECK_EQ(y2016.id_extra[1], 0);
    CHECK_EQ(y2016.id_extra[2], 2);
    CHECK_EQ(y2016.id_extra[3], 0);
    CHECK_EQ(y2019.extra, 1);
    CHECK_EQ(y2019.id_extra[2], 1);
    CHECK_EQ(y2019.id_extra[3], 0);
    CHECK_EQ(y2023.cycles, 12);
    CHECK_EQ(y2023.extra, 4);
    CHECK_EQ(y2023.id_extra[1], 0);
    CHECK_EQ(y2023.id_extra[2], 1);
    CHECK_EQ(y2023.ctrl[3], 3);
    CHECK_EQ(y2014.cycles, 22);
    CHECK_EQ(y2014.extra, 11);
    CHECK_EQ(y2014.id_extra[1], 2);
    CHECK_EQ(y2014.id_extra[2], 2);
    CHECK_EQ(y2014.id_extra[3], 2);
    CHECK_EQ(y2014.id_extra[4], 0);
    CHECK_EQ(y2014.id_extra[5], 2);
    CHECK_EQ(y2014.id_extra[6], 0);
    CHECK_EQ(y2014.ctrl[6], 3);
}

static void test_struct_and_control(void) {
    PipeStat uni;
    PipeStat taken;
    PipeStat fall;
    PipeStat slot;
    if (load_sim("struct", &uni) != 0 || load_sim("predict-taken", &taken) != 0 ||
        load_sim("predict-not", &fall) != 0 || load_sim("slot", &slot) != 0) {
        return;
    }
    CHECK_EQ(uni.cycles, 9);
    CHECK_EQ(uni.extra, 1);
    CHECK_EQ(taken.nflush, 2);
    CHECK_EQ(taken.committed, 2);
    CHECK_EQ(taken.cycles, 8);
    CHECK_EQ(taken.extra, 2);
    CHECK_EQ(fall.nflush, 0);
    CHECK_EQ(fall.cycles, 7);
    CHECK_EQ(fall.extra, 0);
    CHECK_EQ(slot.cycles, 7);
    CHECK_EQ(slot.extra, 0);
    CHECK_EQ(slot.nflush, 0);
}

static void test_raw_pairs(void) {
    Insn buf[INSN_N];
    char err[80];
    const char *y2016 =
        "add R1, R2, R3\nadd R5, R2, R4\nadd R4, R5, R3\nadd R5, R2, R6\n";
    const char *y2019 =
        "add s2, s1, s0\nload s3, 0(t2)\nadd s2, s2, s3\nstore s2, 0(t2)\n";
    const char *y2014 =
        "sll R4, R2, 2\nadd R4, R4, R3\nload R5, 0(R4)\nadd R1, R1, R5\n"
        "add R2, R2, 1\nbne R2, R6, loop\nloop: sll R4, R2, 2\n";
    int n = insn_parse(y2016, buf, INSN_N, err, (int)sizeof err);
    CHECK_EQ(n, 4);
    CHECK_EQ(insn_raw(buf, 1, 2), 1);
    CHECK_EQ(insn_raw(buf, 0, 1), 0);
    CHECK_EQ(insn_raw(buf, 1, 3), 0);
    CHECK_EQ(insn_raw(buf, 2, 3), 0);
    n = insn_parse(y2019, buf, INSN_N, err, (int)sizeof err);
    CHECK_EQ(n, 4);
    CHECK_EQ(insn_raw(buf, 0, 2), 1);
    CHECK_EQ(insn_raw(buf, 1, 2), 1);
    CHECK_EQ(insn_raw(buf, 1, 3), 0);
    CHECK_EQ(insn_raw(buf, 2, 3), 1);
    n = insn_parse(y2014, buf, INSN_N, err, (int)sizeof err);
    CHECK_EQ(n, 7);
    CHECK_EQ(insn_raw(buf, 4, 6), 1);
    CHECK_EQ(insn_parse("add\n", buf, INSN_N, err, (int)sizeof err), -1);
}

int main(void) {
    test_metric();
    test_late();
    test_forward_and_branch();
    test_struct_and_control();
    test_raw_pairs();
    return TEST_SUMMARY();
}
