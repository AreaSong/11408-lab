#include "bubble.h"
#include "insert.h"
#include "quick.h"
#include "seq.h"
#include "shell.h"

#include "test.h"

static void expect_keys(const Seq *s, const int *key, int n) {
    int i;
    CHECK_EQ(s->n, n);
    for (i = 0; i < n; i++) {
        CHECK_EQ(s->a[i].key, key[i]);
    }
}

static void test_moves_match(void) {
    int samples[][8] = {
        {1, 2, 3, 4, 5, 0, 0, 0},
        {5, 4, 3, 2, 1, 0, 0, 0},
        {49, 38, 65, 97, 76, 13, 27, 49},
        {3, 3, 2, 3, 1, 0, 0, 0},
        {8, 3, 9, 11, 2, 1, 4, 7},
    };
    int lens[] = {5, 5, 8, 5, 8};
    int s;
    for (s = 0; s < 5; s++) {
        Seq src;
        Seq direct[SORT_MAX];
        Seq binary[SORT_MAX];
        int nd;
        int nb;
        int i;
        seq_load(&src, samples[s], lens[s]);
        nd = insert_collect(&src, direct, SORT_MAX, 0);
        nb = insert_collect(&src, binary, SORT_MAX, 1);
        CHECK_EQ(nd, nb);
        CHECK_EQ(nd, lens[s] - 1);
        for (i = 0; i < nd; i++) {
            int j;
            CHECK_EQ(direct[i].n, binary[i].n);
            for (j = 0; j < direct[i].n; j++) {
                CHECK_EQ(direct[i].a[j].key, binary[i].a[j].key);
                CHECK_EQ(direct[i].a[j].ord, binary[i].a[j].ord);
            }
            CHECK_EQ(direct[i].move, binary[i].move);
        }
    }
    {
        int up[] = {1, 2, 3, 4, 5};
        int down[] = {5, 4, 3, 2, 1};
        Seq src;
        Seq out[SORT_MAX];
        int nd;
        long up_cmp;
        seq_load(&src, up, 5);
        nd = insert_collect(&src, out, SORT_MAX, 0);
        CHECK_EQ(out[nd - 1].cmp, 4);
        nd = insert_collect(&src, out, SORT_MAX, 1);
        up_cmp = out[nd - 1].cmp;
        CHECK(up_cmp > 4);
        seq_load(&src, down, 5);
        nd = insert_collect(&src, out, SORT_MAX, 1);
        CHECK(out[nd - 1].cmp != up_cmp);
    }
}

static void test_shell_known(void) {
    int classic[] = {49, 38, 65, 97, 76, 13, 27, 49};
    int g4[] = {49, 13, 27, 49, 76, 38, 65, 97};
    int g2[] = {27, 13, 49, 38, 65, 49, 76, 97};
    int g1[] = {13, 27, 38, 49, 49, 65, 76, 97};
    int y2018[] = {8, 3, 9, 11, 2, 1, 4, 7, 5, 10, 6};
    int p1[] = {1, 3, 7, 5, 2, 6, 4, 9, 11, 10, 8};
    int p2[] = {1, 2, 6, 4, 3, 7, 5, 8, 11, 10, 9};
    int y2025[] = {5, 25, 40, 30, 10, 20, 45, 15, 35};
    int s1[] = {5, 10, 20, 30, 15, 35, 45, 25, 40};
    int s2[] = {5, 10, 15, 25, 20, 30, 40, 35, 45};
    int witness[] = {4, 1, 3, 2};
    Seq s;
    seq_load(&s, classic, 8);
    shell_pass(&s, 4);
    expect_keys(&s, g4, 8);
    shell_pass(&s, 2);
    expect_keys(&s, g2, 8);
    shell_pass(&s, 1);
    expect_keys(&s, g1, 8);
    seq_load(&s, y2018, 11);
    shell_pass(&s, 5);
    expect_keys(&s, p1, 11);
    shell_pass(&s, 3);
    expect_keys(&s, p2, 11);
    seq_load(&s, y2025, 9);
    shell_pass(&s, 3);
    expect_keys(&s, s1, 9);
    shell_pass(&s, 2);
    expect_keys(&s, s2, 9);
    seq_load(&s, witness, 4);
    shell_pass(&s, 2);
    CHECK_EQ(seq_fixed(&s), 0);
}

static void test_sink_2010(void) {
    int init[] = {2, 12, 16, 88, 5, 10};
    int p1[] = {2, 12, 16, 5, 10, 88};
    int p2[] = {2, 12, 5, 10, 16, 88};
    int p3[] = {2, 5, 10, 12, 16, 88};
    int rise1[] = {2, 5, 12, 16, 88, 10};
    Seq src;
    Seq out[SORT_MAX];
    int n;
    seq_load(&src, init, 6);
    n = bubble_collect(&src, out, SORT_MAX, 1);
    CHECK(n >= 3);
    expect_keys(&out[0], p1, 6);
    expect_keys(&out[1], p2, 6);
    expect_keys(&out[2], p3, 6);
    n = bubble_collect(&src, out, SORT_MAX, 0);
    CHECK(n >= 1);
    expect_keys(&out[0], rise1, 6);
}

static int has_two(const Seq *s, int lo, int hi) {
    int i;
    int j;
    for (i = lo; i <= hi; i++) {
        for (j = i + 1; j <= hi; j++) {
            if (s->a[i].key == s->a[j].key) {
                return 1;
            }
        }
    }
    return 0;
}

static void test_quick_classic(void) {
    int init[] = {49, 38, 65, 97, 76, 13, 27, 49};
    int cut[] = {27, 38, 13, 49, 76, 97, 65, 49};
    int span[] = {2, 1, 3, 4, 5, 6, 7, 8};
    int dup[] = {3, 5, 5, 1, 4, 2};
    Seq src;
    Seq out[SORT_MAX];
    Seq s;
    int at;
    int i;
    seq_load(&src, init, 8);
    CHECK(quick_collect(&src, out, SORT_MAX) >= 1);
    expect_keys(&out[0], cut, 8);
    CHECK(seq_final_at(&out[0], 3));
    seq_load(&s, span, 8);
    at = quick_partition(&s, 0, 7);
    CHECK_EQ(at, 1);
    CHECK_EQ(s.a[at].key, 2);
    seq_load(&s, dup, 6);
    at = quick_partition(&s, 0, 5);
    CHECK(seq_final_at(&s, at));
    CHECK(has_two(&s, at + 1, s.n - 1));
    for (i = 0; i < 6; i++) {
        int small[][6] = {
            {1, 2, 3, 4, 5, 6}, {6, 5, 4, 3, 2, 1}, {3, 1, 4, 1, 5, 9},
            {2, 2, 2, 2, 2, 2}, {9, 1, 8, 2, 7, 3}, {4, 4, 1, 4, 2, 4},
        };
        seq_load(&s, small[i], 6);
        at = quick_partition(&s, 0, 5);
        CHECK(seq_final_at(&s, at));
    }
}

static void test_quick_possible(void) {
    int a[] = {2, 3, 5, 4, 6, 7, 9};
    int b[] = {2, 7, 5, 6, 4, 3, 9};
    int c[] = {3, 2, 5, 4, 7, 6, 9};
    int d[] = {4, 2, 3, 5, 7, 6, 9};
    int e[] = {5, 2, 16, 12, 28, 60, 32, 72};
    int f[] = {2, 16, 5, 28, 12, 60, 32, 72};
    int g[] = {2, 12, 16, 5, 28, 32, 72, 60};
    int h[] = {5, 2, 12, 28, 16, 32, 72, 60};
    int y09[] = {11, 12, 13, 7, 8, 9, 23, 4, 5};
    int pivot[] = {68, 11, 70, 23, 80, 77, 48, 81, 93, 88};
    Seq s;
    int i;
    int finals = 0;
    seq_load(&s, a, 7);
    CHECK(quick_possible(&s, 2));
    seq_load(&s, b, 7);
    CHECK(quick_possible(&s, 2));
    seq_load(&s, c, 7);
    CHECK(!quick_possible(&s, 2));
    seq_load(&s, d, 7);
    CHECK(quick_possible(&s, 2));
    seq_load(&s, e, 8);
    CHECK(quick_possible(&s, 2));
    seq_load(&s, f, 8);
    CHECK(quick_possible(&s, 2));
    seq_load(&s, g, 8);
    CHECK(quick_possible(&s, 2));
    seq_load(&s, h, 8);
    CHECK(!quick_possible(&s, 2));
    seq_load(&s, y09, 9);
    CHECK(!quick_possible(&s, 2));
    seq_load(&s, pivot, 10);
    for (i = 0; i < 10; i++) {
        if (seq_final_at(&s, i)) {
            finals++;
            CHECK_EQ(s.a[i].key, 81);
        }
    }
    CHECK_EQ(finals, 1);
}

static void test_stable_and_calls(void) {
    int shell_key[] = {2, 2, 1};
    int plain[] = {3, 3, 2};
    int sorted[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int mixed[] = {49, 38, 65, 97, 76, 13, 27, 49};
    Seq s;
    Seq out[SORT_MAX];
    long p0;
    long p1;
    long parts;
    int depth;
    int i;
    int n;
    seq_load(&s, shell_key, 3);
    shell_pass(&s, 2);
    CHECK(!seq_stable(&s));
    seq_load(&s, plain, 3);
    n = insert_collect(&s, out, SORT_MAX, 0);
    CHECK(seq_stable(&out[n - 1]));
    seq_load(&s, plain, 3);
    n = quick_collect(&s, out, SORT_MAX);
    CHECK(!seq_stable(&out[n - 1]));
    seq_load(&s, sorted, 8);
    quick_recur(&s, Q_LEFT, &p0, &depth);
    for (i = 1; i < 4; i++) {
        quick_recur(&s, i, &p1, &depth);
        CHECK_EQ(p0, p1);
    }
    quick_opt(&s, &parts, &depth);
    CHECK_EQ(parts, p0);
    CHECK(depth < 8);
    seq_load(&s, mixed, 8);
    quick_recur(&s, Q_LEFT, &p1, &depth);
    CHECK(p0 != p1);
}

static void test_one(void) {
    int key[] = {7};
    Seq s;
    Seq out[SORT_MAX];
    seq_load(&s, key, 1);
    CHECK_EQ(insert_collect(&s, out, SORT_MAX, 0), 0);
    CHECK_EQ(insert_collect(&s, out, SORT_MAX, 1), 0);
    CHECK_EQ(bubble_collect(&s, out, SORT_MAX, 1), 0);
    CHECK_EQ(quick_collect(&s, out, SORT_MAX), 0);
    CHECK(seq_sorted(&s));
}

int main(void) {
    test_moves_match();
    test_shell_known();
    test_sink_2010();
    test_quick_classic();
    test_quick_possible();
    test_stable_and_calls();
    test_one();
    return TEST_SUMMARY();
}
