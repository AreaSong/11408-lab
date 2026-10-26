#include "asl.h"
#include "bin.h"
#include "blk.h"
#include "seq.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int capture(FILE *fp, char *buf, int cap) {
    long n;
    fflush(fp);
    n = ftell(fp);
    if (n < 0 || n >= cap) {
        return 0;
    }
    rewind(fp);
    if (n > 0 && fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        return 0;
    }
    buf[n] = '\0';
    return 1;
}

static void walk_fail(const BinTable *t, int node, int *mx, long long *sum, int *cnt) {
    if (node == 0) {
        return;
    }
    if (t->left[node] == 0) {
        if (t->depth[node] > *mx) {
            *mx = t->depth[node];
        }
        *sum += t->depth[node];
        (*cnt)++;
    } else {
        walk_fail(t, t->left[node], mx, sum, cnt);
    }
    if (t->right[node] == 0) {
        if (t->depth[node] > *mx) {
            *mx = t->depth[node];
        }
        *sum += t->depth[node];
        (*cnt)++;
    } else {
        walk_fail(t, t->right[node], mx, sum, cnt);
    }
}

static void test_seq_formula(void) {
    SeqTable t;
    AslPair a;
    int keys[] = {10, 20, 30, 40, 50, 60};
    int cmp = 0;
    CHECK(seq_load(&t, SEQ_PLAIN, keys, 6) == S_OK);
    seq_asl(&t, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 7, 2));
    CHECK(asl_same(a.fail_n, a.fail_d, 6, 1));
    CHECK(seq_load(&t, SEQ_GUARD, keys, 6) == S_OK);
    seq_asl(&t, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 7, 2));
    CHECK_EQ(a.fail_n, 7);
    CHECK_EQ(seq_find(&t, 20, NULL, &cmp), 2);
    CHECK_EQ(cmp, 5);
    CHECK_EQ(seq_find(&t, 1, NULL, &cmp), 0);
    CHECK_EQ(cmp, 7);
    CHECK(seq_load(&t, SEQ_ORDER, keys, 6) == S_OK);
    seq_asl(&t, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 7, 2));
    CHECK(asl_same(a.fail_n, a.fail_d, 27, 7));
    CHECK_EQ(seq_find(&t, 25, NULL, &cmp), 0);
    CHECK_EQ(cmp, 3);
    CHECK_EQ(seq_find(&t, 40, NULL, &cmp), 4);
    CHECK_EQ(cmp, 4);
    CHECK_EQ(seq_find(&t, 5, NULL, &cmp), 0);
    CHECK_EQ(cmp, 1);
    CHECK_EQ(seq_find(&t, 70, NULL, &cmp), 0);
    CHECK_EQ(cmp, 6);
    CHECK(seq_load(&t, SEQ_PLAIN, keys, 6) == S_OK);
    CHECK_EQ(seq_find(&t, 20, NULL, &cmp), 2);
    CHECK_EQ(cmp, 2);
    CHECK_EQ(seq_find(&t, 1, NULL, &cmp), 0);
    CHECK_EQ(cmp, 6);
}

static void test_weight(void) {
    SeqTable s;
    BinTable b;
    AslPair a;
    int sorted[] = {1, 2, 3, 4};
    int pb[] = {35, 15, 15, 35};
    int rearranged[] = {1, 4, 2, 3};
    int ps[] = {35, 35, 15, 15};
    int zero[] = {1, 0, 1, 1, 1, 1};
    int keys6[] = {10, 20, 30, 40, 50, 60};
    CHECK(bin_load(&b, 0, sorted, 4) == S_OK);
    CHECK(bin_weight(&b, pb, 4) == S_OK);
    bin_asl(&b, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 220, 100));
    CHECK(seq_load(&s, SEQ_PLAIN, rearranged, 4) == S_OK);
    CHECK(seq_weight(&s, ps, 4) == S_OK);
    seq_asl(&s, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 210, 100));
    CHECK(seq_load(&s, SEQ_ORDER, keys6, 6) == S_OK);
    CHECK(seq_weight(&s, zero, 6) == S_RANGE);
    CHECK_EQ(s.weighted, 0);
}

static void test_bin_counts(void) {
    BinTable t;
    AslPair a;
    int keys[32];
    int i;
    int mx;
    int cnt;
    long long sum;
    for (i = 0; i < 5; i++) {
        keys[i] = i + 1;
    }
    CHECK(bin_load(&t, 0, keys, 5) == S_OK);
    bin_asl(&t, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 11, 5));
    CHECK(asl_same(a.fail_n, a.fail_d, 16, 6));
    CHECK_EQ(t.height, 3);
    for (i = 0; i < 11; i++) {
        keys[i] = i + 1;
    }
    CHECK(bin_load(&t, 0, keys, 11) == S_OK);
    bin_asl(&t, &a);
    CHECK(asl_same(a.succ_n, a.succ_d, 33, 11));
    CHECK(asl_same(a.fail_n, a.fail_d, 44, 12));
    CHECK_EQ(t.height, 4);
    mx = 0;
    cnt = 0;
    sum = 0;
    walk_fail(&t, t.root, &mx, &sum, &cnt);
    CHECK_EQ(mx, 4);
    CHECK_EQ(cnt, 12);
    CHECK_EQ(sum, 44);
}

static void test_height(void) {
    int n;
    int h = 0;
    CHECK(bin_tree_sum(16, 0, NULL, &h) == S_OK);
    CHECK_EQ(h, 5);
    CHECK_EQ(bin_bound(16), 5);
    CHECK(bin_tree_sum(600, 0, NULL, &h) == S_OK);
    CHECK_EQ(h, 10);
    CHECK_EQ(bin_bound(600), 10);
    CHECK(bin_tree_sum(600, 1, NULL, &h) == S_OK);
    CHECK_EQ(h, 10);
    for (n = 1; n <= 128; n++) {
        int floor_h = 0;
        int ceil_h = 0;
        int bad = 0;
        if (bin_tree_sum(n, 0, NULL, &floor_h) != S_OK || floor_h != bin_bound(n)) {
            bad = n;
        }
        if (bin_tree_sum(n, 1, NULL, &ceil_h) != S_OK || ceil_h != bin_bound(n)) {
            bad = n;
        }
        CHECK_EQ(bad, 0);
    }
}

static void test_mirror(void) {
    int floor_d[65];
    int ceil_d[65];
    int n;
    CHECK(bin_fill_depth(2, 0, floor_d) == S_OK);
    CHECK(bin_fill_depth(2, 1, ceil_d) == S_OK);
    CHECK(floor_d[1] != ceil_d[1]);
    for (n = 1; n <= 64; n++) {
        int hf[16];
        int hc[16];
        int i;
        int bad = 0;
        CHECK(bin_fill_depth(n, 0, floor_d) == S_OK);
        CHECK(bin_fill_depth(n, 1, ceil_d) == S_OK);
        memset(hf, 0, sizeof hf);
        memset(hc, 0, sizeof hc);
        for (i = 1; i <= n; i++) {
            if (floor_d[i] < 1 || floor_d[i] >= 16 || ceil_d[i] < 1 || ceil_d[i] >= 16) {
                bad = i;
            }
            hf[floor_d[i] > 0 && floor_d[i] < 16 ? floor_d[i] : 0]++;
            hc[ceil_d[i] > 0 && ceil_d[i] < 16 ? ceil_d[i] : 0]++;
        }
        for (i = 0; i < 16; i++) {
            if (hf[i] != hc[i]) {
                bad = n;
            }
        }
        CHECK_EQ(bad, 0);
    }
}

static void test_path_fit(void) {
    int bad[] = {500, 200, 450, 180};
    int b[] = {500, 450, 200, 180};
    int c[] = {180, 500, 200, 450};
    int d[] = {180, 200, 500, 450};
    FILE *fp = tmpfile();
    CHECK(fp != NULL);
    if (fp != NULL) {
        CHECK_EQ(bin_path(bad, 4, fp), 0);
        CHECK_EQ(bin_path(b, 4, fp), 1);
        CHECK_EQ(bin_path(c, 4, fp), 1);
        CHECK_EQ(bin_path(d, 4, fp), 1);
        fclose(fp);
    }
    CHECK_EQ(bin_suitable("array", "order"), 1);
    CHECK_EQ(bin_suitable("list", "order"), 0);
    CHECK_EQ(bin_suitable("array", "chaos"), 0);
    CHECK_EQ(bin_suitable("static", "order"), 0);
    CHECK_EQ(bin_suitable("static", "chaos"), 0);
    CHECK_EQ(bin_suitable("tree", "order"), -1);
}

static void test_best(void) {
    int sizes[] = {8, 10, 20, 25};
    BlkFrac f;
    BestAsk ask;
    FILE *fp = tmpfile();
    char buf[500];
    CHECK(blk_formula(0, 0, 50, 8, &f) == S_OK);
    CHECK(asl_same(f.num, f.den, 30, 1));
    CHECK(blk_formula(0, 0, 40, 10, &f) == S_OK);
    CHECK(asl_same(f.num, f.den, 26, 1));
    CHECK(blk_formula(0, 0, 20, 20, &f) == S_OK);
    CHECK(asl_same(f.num, f.den, 21, 1));
    CHECK(blk_formula(0, 0, 16, 25, &f) == S_OK);
    CHECK(asl_same(f.num, f.den, 43, 2));
    ask.index_bin = 0;
    ask.inner_bin = 0;
    ask.n = 400;
    ask.sizes = sizes;
    ask.ns = 4;
    CHECK(fp != NULL);
    if (fp == NULL) {
        return;
    }
    CHECK(blk_best(&ask, fp) == S_OK);
    CHECK(capture(fp, buf, (int)sizeof buf));
    CHECK(strstr(buf, "pick s=20 asl=21/1=21.00 tie=-") != NULL);
    fclose(fp);
}

static int add_pair(BlkTable *t, int x, int y) {
    int k[2];
    k[0] = x;
    k[1] = y;
    return blk_add(t, k, 2);
}

static void test_block_numbers(void) {
    BlkTable t;
    BlkAsl a;
    int cmp = 0;
    int bad[] = {2, 1};
    int overlap[] = {3, 2};
    blk_begin(&t, 0, 0);
    CHECK(add_pair(&t, 1, 3) == S_OK);
    CHECK(add_pair(&t, 4, 6) == S_OK);
    CHECK(add_pair(&t, 7, 9) == S_OK);
    blk_measure(&t, &a);
    CHECK(asl_same(a.exact_n, a.exact_d, 21, 6));
    CHECK(asl_same(a.rough_n, a.rough_d, 7, 2));
    blk_begin(&t, 1, 0);
    CHECK(add_pair(&t, 1, 3) == S_OK);
    CHECK(add_pair(&t, 4, 6) == S_OK);
    CHECK(add_pair(&t, 7, 9) == S_OK);
    blk_measure(&t, &a);
    CHECK(asl_same(a.exact_n, a.exact_d, 20, 6));
    CHECK(asl_same(a.rough_n, a.rough_d, 19, 6));
    CHECK(!asl_same(a.exact_n, a.exact_d, a.rough_n, a.rough_d));
    CHECK_EQ(blk_find(&t, 4, NULL, &cmp), 1);
    CHECK_EQ(cmp, 3);
    CHECK_EQ(blk_find(&t, 3, NULL, &cmp), 2);
    CHECK_EQ(cmp, 4);
    CHECK_EQ(blk_find(&t, 10, NULL, &cmp), 0);
    CHECK_EQ(cmp, 2);
    blk_begin(&t, 0, 1);
    CHECK(blk_add(&t, bad, 2) == S_ORDER);
    CHECK_EQ(t.b, 0);
    blk_begin(&t, 0, 0);
    CHECK(blk_add(&t, (int[]){5, 1, 4}, 3) == S_OK);
    CHECK(blk_add(&t, overlap, 2) == S_ORDER);
    CHECK_EQ(t.b, 1);
}

static void test_block_inner_bin(void) {
    BlkTable t;
    BlkAsl a;
    int i;
    blk_begin(&t, 0, 1);
    for (i = 0; i < 3; i++) {
        int k[3];
        k[0] = i * 3 + 1;
        k[1] = i * 3 + 2;
        k[2] = i * 3 + 3;
        CHECK(blk_add(&t, k, 3) == S_OK);
    }
    blk_measure(&t, &a);
    CHECK(asl_same(a.exact_n, a.exact_d, 33, 9));
    CHECK(asl_same(a.rough_n, a.rough_d, 11, 3));
}

int main(void) {
    test_seq_formula();
    test_weight();
    test_bin_counts();
    test_height();
    test_mirror();
    test_path_fit();
    test_best();
    test_block_numbers();
    test_block_inner_bin();
    return TEST_SUMMARY();
}
