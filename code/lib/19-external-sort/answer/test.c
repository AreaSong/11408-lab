#include "balance.h"
#include "best.h"
#include "loser.h"
#include "repl.h"

#include "test.h"

#include <string.h>

static void expect_i(const int *got, const int *want, int n) {
    int i;
    for (i = 0; i < n; i++) {
        CHECK_EQ(got[i], want[i]);
    }
}

static void test_passes(void) {
    CHECK_EQ(merge_passes(625, 5), 4);
    CHECK_EQ(balance_io(375000, 4), 3000000);
    CHECK_EQ(merge_passes(80, 2), 7);
    CHECK_EQ(merge_passes(80, 10), 2);
    CHECK_EQ(merge_passes(10, 3), 3);
    CHECK_EQ(merge_passes(9, 3), 2);
    CHECK_EQ(merge_passes(1, 5), 0);
    CHECK_EQ(balance_io(0, 0), 0);
    CHECK_EQ(plain_runs(375000, 600), 625);
    CHECK_EQ(plain_runs(10, 3), 4);
    CHECK_EQ(plain_runs(5, 5), 1);
    CHECK_EQ(dummy_count(120, 12), 2);
    CHECK_EQ(dummy_count(9, 3), 0);
    CHECK_EQ(dummy_count(8, 3), 1);
    CHECK_EQ(dummy_count(2, 2), 0);
    CHECK_EQ(dummy_count(6, 4), 1);
}

static void test_kcmp(void) {
    SelectBound bound;
    CHECK_EQ(select_bounds(100, 5, &bound), 0);
    CHECK_EQ(bound.path, 3);
    CHECK_EQ(bound.formula, 300);
    CHECK_EQ(bound.naive_rest, 396);
    CHECK_EQ(bound.naive_all, 400);
    CHECK_EQ(select_bounds(1, 2, &bound), 0);
    CHECK_EQ(bound.path, 1);
    CHECK_EQ(bound.formula, 1);
    CHECK_EQ(bound.naive_rest, 0);
}

static void test_best_nine(void) {
    static const long long len[] = {9, 30, 12, 18, 3, 17, 2, 6, 24};
    static const long long made[] = {11, 32, 59, 121};
    BestTree tree;
    int i;
    CHECK_EQ(best_tree(3, len, 9, &tree), 0);
    CHECK_EQ(tree.dummy, 0);
    CHECK_EQ(tree.steps, 4);
    CHECK_EQ(tree.wpl, 223);
    CHECK_EQ(tree.io, 446);
    for (i = 0; i < 4; i++) {
        CHECK_EQ(tree.made[i], made[i]);
    }
    CHECK_EQ(tree.part[0][0], 2);
    CHECK_EQ(tree.part[0][1], 3);
    CHECK_EQ(tree.part[0][2], 6);
    CHECK_EQ(tree.part[3][0], 30);
    CHECK_EQ(tree.part[3][1], 32);
    CHECK_EQ(tree.part[3][2], 59);
}

static void test_best_dummy(void) {
    static const long long len[] = {9, 12, 18, 3, 17, 2, 6, 24};
    static const long long tie[] = {5, 5, 5, 1};
    static const long long pair[] = {4, 5};
    BestTree tree;
    CHECK_EQ(best_tree(3, len, 8, &tree), 0);
    CHECK_EQ(tree.dummy, 1);
    CHECK_EQ(tree.wpl, 163);
    CHECK_EQ(tree.io, 326);
    CHECK_EQ(tree.part[0][0], 0);
    CHECK_EQ(tree.made[0], 5);
    CHECK_EQ(tree.made[3], 91);
    CHECK_EQ(best_tree(3, tie, 4, &tree), 0);
    CHECK_EQ(tree.dummy, 1);
    CHECK_EQ(tree.wpl, 22);
    CHECK_EQ(best_tree(3, pair, 2, &tree), 0);
    CHECK_EQ(tree.dummy, 1);
    CHECK_EQ(tree.wpl, 9);
}

static void test_best_2012(void) {
    static const long long len[] = {10, 35, 40, 50, 60, 200};
    static const long long made[] = {45, 85, 110, 195, 395};
    BestTree tree;
    int i;
    CHECK_EQ(best_tree(2, len, 6, &tree), 0);
    CHECK_EQ(tree.dummy, 0);
    CHECK_EQ(tree.steps, 5);
    CHECK_EQ(tree.wpl, 830);
    CHECK_EQ(tree.io, 1660);
    CHECK_EQ(tree.cmp_worst, 825);
    for (i = 0; i < 5; i++) {
        CHECK_EQ(tree.made[i], made[i]);
    }
}

static void fill_one(LoseIn *in, int seg, int key) {
    in->len[seg] = 1;
    in->data[seg][0] = key;
}

static void test_lose(void) {
    LoseIn in;
    LoseResult result;
    static const int keys[] = {1, 4, 2, 6, 3, 5};
    static const int merged[] = {1, 2, 3, 4, 5, 6};
    memset(&in, 0, sizeof(in));
    in.k = 3;
    fill_one(&in, 0, 10);
    fill_one(&in, 1, 3);
    fill_one(&in, 2, 7);
    CHECK_EQ(loser_merge(&in, &result), 0);
    CHECK_EQ(result.winner_seg, 1);
    CHECK_EQ(result.winner_key, 3);
    CHECK_EQ(result.ls[0], 1);
    CHECK_EQ(result.ls[1], 0);
    CHECK_EQ(result.ls[2], 2);
    CHECK_EQ(result.n, 3);
    CHECK_EQ(result.out[0], 3);
    CHECK_EQ(result.out[1], 7);
    CHECK_EQ(result.out[2], 10);
    CHECK_EQ(result.cmp, 10);
    CHECK_EQ(result.path, 2);
    CHECK_EQ(result.formula, 6);

    memset(&in, 0, sizeof(in));
    in.k = 3;
    in.len[0] = in.len[1] = in.len[2] = 2;
    in.data[0][0] = keys[0];
    in.data[0][1] = keys[1];
    in.data[1][0] = keys[2];
    in.data[1][1] = keys[3];
    in.data[2][0] = keys[4];
    in.data[2][1] = keys[5];
    CHECK_EQ(loser_merge(&in, &result), 0);
    CHECK_EQ(result.winner_seg, 0);
    CHECK_EQ(result.cmp, 15);
    expect_i(result.out, merged, 6);

    memset(&in, 0, sizeof(in));
    in.k = 2;
    in.len[0] = 2;
    in.data[0][0] = in.data[0][1] = 5;
    in.len[1] = 1;
    in.data[1][0] = 5;
    CHECK_EQ(loser_merge(&in, &result), 0);
    CHECK_EQ(result.from[0], 0);
    CHECK_EQ(result.from[1], 0);
    CHECK_EQ(result.from[2], 1);
}

static void test_repl(void) {
    static const int classic[] = {51, 49, 39, 46, 38, 29, 14, 61, 15, 30, 1};
    static const int classic_out[] = {39, 46, 49, 51, 14, 29, 38, 61, 1, 15, 30};
    static const int y2023[] = {51, 94, 37, 92, 14, 63, 15, 99, 48, 56, 23, 60,
                                31, 17, 43, 8,  90, 166, 100};
    static const int y2023_out[] = {37, 51, 63, 92, 94, 99, 14, 15, 23, 31, 48, 56,
                                    60, 90, 166, 8,  17, 43, 100};
    static const int up[] = {1, 2, 3, 4, 5};
    static const int down[] = {5, 4, 3, 2, 1};
    static const int tie[] = {2, 1, 2};
    static const int tie_ord[] = {1, 0, 2};
    Repl result;
    CHECK_EQ(repl_select(3, classic, 11, &result), 0);
    CHECK_EQ(result.runs, 3);
    CHECK_EQ(result.first, 4);
    CHECK_EQ(result.len[0], 4);
    CHECK_EQ(result.len[1], 4);
    CHECK_EQ(result.len[2], 3);
    expect_i(result.key, classic_out, 11);
    CHECK_EQ(repl_select(4, y2023, 19, &result), 0);
    CHECK_EQ(result.runs, 3);
    CHECK_EQ(result.first, 6);
    CHECK_EQ(result.bound_max, 19);
    CHECK_EQ(result.bound_min, 4);
    CHECK_EQ(result.len[0], 6);
    CHECK_EQ(result.len[1], 9);
    CHECK_EQ(result.len[2], 4);
    expect_i(result.key, y2023_out, 19);
    CHECK_EQ(repl_select(3, up, 5, &result), 0);
    CHECK_EQ(result.runs, 1);
    CHECK_EQ(result.first, 5);
    CHECK_EQ(repl_select(3, down, 5, &result), 0);
    CHECK_EQ(result.runs, 2);
    CHECK_EQ(result.first, 3);
    CHECK_EQ(result.key[0], 3);
    CHECK_EQ(result.key[3], 1);
    CHECK_EQ(repl_select(2, tie, 3, &result), 0);
    CHECK_EQ(result.runs, 1);
    expect_i(result.ord, tie_ord, 3);
    CHECK_EQ(repl_select(5, tie, 3, &result), 0);
    CHECK_EQ(result.runs, 1);
    CHECK_EQ(result.bound_min, 3);
    CHECK_EQ(result.first, 3);
}

int main(void) {
    test_passes();
    test_kcmp();
    test_best_nine();
    test_best_dummy();
    test_best_2012();
    test_lose();
    test_repl();
    return TEST_SUMMARY();
}
