#include "cost.h"
#include "hsort.h"
#include "merge.h"
#include "meter.h"
#include "quick.h"
#include "radix.h"
#include "select.h"
#include "shape.h"
#include "shell.h"

#include "test.h"

static void expect_keys(const Seq *s, const int *key, int n) {
    int i;
    CHECK_EQ(s->n, n);
    for (i = 0; i < n; i++) {
        CHECK_EQ(s->a[i].key, key[i]);
    }
}

static void test_select_counts(void) {
    int up[] = {1, 2, 3, 4, 5};
    int down[] = {5, 4, 3, 2, 1};
    int worst[] = {2, 3, 4, 5, 1};
    int classic[] = {49, 38, 65, 97, 76, 13, 27, 49};
    int pass1[] = {13, 38, 65, 97, 76, 49, 27, 49};
    int pair[] = {3, 3, 2};
    Seq s;
    seq_load(&s, up, 5);
    select_sort(&s, 0, 0);
    CHECK_EQ(s.cmp, 10);
    CHECK_EQ(s.move, 0);
    seq_load(&s, down, 5);
    select_sort(&s, 0, 0);
    CHECK_EQ(s.cmp, 10);
    CHECK_EQ(s.move, 6);
    CHECK(seq_sorted(&s));
    seq_load(&s, worst, 5);
    select_sort(&s, 0, 0);
    CHECK_EQ(s.move, 12);
    seq_load(&s, classic, 8);
    select_sort(&s, 1, 0);
    expect_keys(&s, pass1, 8);
    seq_load(&s, pair, 3);
    select_sort(&s, 0, 0);
    CHECK(seq_sorted(&s));
    CHECK(!seq_stable(&s));
}

static void test_heap_passes(void) {
    int classic[] = {49, 38, 65, 97, 76, 13, 27, 49};
    int pass1[] = {76, 49, 65, 38, 49, 13, 27, 97};
    int sorted[] = {13, 27, 38, 49, 49, 65, 76, 97};
    int heap24[] = {28, 22, 20, 19, 8, 12, 15, 5};
    int left[] = {20, 19, 15, 5, 8, 12};
    int y2018[] = {6, 1, 5, 9, 8, 4, 7};
    Seq s;
    seq_load(&s, classic, 8);
    CHECK(hsort_sort(&s, 1, 0));
    expect_keys(&s, pass1, 8);
    CHECK_EQ(s.a[1].ord, 7);
    CHECK_EQ(s.a[4].ord, 0);
    seq_load(&s, classic, 8);
    CHECK(hsort_sort(&s, 0, 0));
    expect_keys(&s, sorted, 8);
    {
        int tie[] = {1, 1, 2};
        seq_load(&s, tie, 3);
        CHECK(hsort_sort(&s, 0, 0));
        CHECK(seq_sorted(&s));
        CHECK(!seq_stable(&s));
        CHECK_EQ(s.a[0].ord, 1);
        CHECK_EQ(s.a[1].ord, 0);
    }
    seq_load(&s, heap24, 8);
    CHECK(hsort_sort(&s, 2, 0));
    CHECK_EQ(s.n, 8);
    for (int i = 0; i < 6; i++) {
        CHECK_EQ(s.a[i].key, left[i]);
    }
    CHECK_EQ(s.a[6].key, 22);
    CHECK_EQ(s.a[7].key, 28);
    seq_load(&s, y2018, 7);
    CHECK(hsort_sort(&s, 0, 0));
    CHECK(seq_sorted(&s));
}

static void test_merge_passes(void) {
    int classic[] = {49, 38, 65, 97, 76, 13, 27, 49};
    int pass1[] = {38, 49, 65, 97, 13, 76, 27, 49};
    int pass2[] = {38, 49, 65, 97, 13, 27, 49, 76};
    int none[] = {3, 8, 1, 4};
    int stayed[] = {3, 8, 1, 4};
    int pair[] = {3, 3, 2};
    Seq s;
    seq_load(&s, classic, 8);
    merge_sort(&s, 1, 0);
    expect_keys(&s, pass1, 8);
    seq_load(&s, classic, 8);
    merge_sort(&s, 2, 0);
    expect_keys(&s, pass2, 8);
    seq_load(&s, classic, 8);
    merge_sort(&s, 0, 0);
    CHECK(seq_sorted(&s));
    CHECK(seq_stable(&s));
    seq_load(&s, none, 4);
    merge_sort(&s, 1, 0);
    expect_keys(&s, stayed, 4);
    CHECK_EQ(seq_fixed(&s), 0);
    seq_load(&s, pair, 3);
    merge_sort(&s, 0, 0);
    CHECK(seq_stable(&s));
}

static void test_radix_passes(void) {
    int y2013[] = {110, 119, 7, 911, 114, 120, 122};
    int pass2[] = {7, 110, 911, 114, 119, 120, 122};
    int yan[] = {329, 457, 657, 839, 436, 720, 355};
    int yan1[] = {720, 355, 436, 457, 657, 329, 839};
    int y2021[] = {93, 946, 372, 9, 146, 151, 301, 485, 236, 327, 43, 892};
    int neg[] = {-3, 4};
    int pair[] = {21, 11, 21};
    Seq s;
    seq_load(&s, y2013, 7);
    CHECK_EQ(radix_sort(&s, 2, 0), 0);
    expect_keys(&s, pass2, 7);
    seq_load(&s, y2013, 7);
    CHECK_EQ(radix_sort(&s, 0, 0), 0);
    CHECK(seq_sorted(&s));
    CHECK_EQ(s.cmp, 0);
    CHECK_EQ(s.move, 42);
    seq_load(&s, yan, 7);
    CHECK_EQ(radix_sort(&s, 1, 0), 0);
    expect_keys(&s, yan1, 7);
    seq_load(&s, y2021, 12);
    CHECK_EQ(radix_sort(&s, 1, 0), 0);
    CHECK_EQ(s.a[1].key, 301);
    CHECK_EQ(s.a[2].key, 372);
    CHECK_EQ(s.a[3].key, 892);
    seq_load(&s, neg, 2);
    CHECK_EQ(radix_sort(&s, 0, 0), -1);
    CHECK_EQ(s.a[0].key, -3);
    seq_load(&s, pair, 3);
    CHECK_EQ(radix_sort(&s, 0, 0), 0);
    CHECK(seq_sorted(&s));
    CHECK(seq_stable(&s));
}

static void test_merge_lists(void) {
    int flat[] = {3, 5, 7, 9, 6};
    int len[] = {2, 2, 1};
    int out[8];
    int expect[] = {3, 5, 6, 7, 9};
    long cmp = 0;
    int n;
    int i;
    n = merge_chain(flat, len, 3, out, &cmp, 0);
    CHECK_EQ(n, 5);
    CHECK_EQ(cmp, 5);
    for (i = 0; i < 5; i++) {
        CHECK_EQ(out[i], expect[i]);
    }
}

static void test_shape_and_stable(void) {
    int y2009[] = {11, 12, 13, 7, 8, 9, 23, 4, 5};
    int shell_key[] = {2, 2, 1};
    int quick_key[] = {3, 3, 1};
    Seq s;
    seq_load(&s, y2009, 9);
    CHECK_EQ(shape_letter(&s, 2), 'B');
    seq_load(&s, shell_key, 3);
    shell_pass(&s, 2);
    CHECK(!seq_stable(&s));
    seq_load(&s, quick_key, 3);
    quick_partition(&s, 0, 2);
    CHECK(!seq_stable(&s));
    CHECK(live_stable("insert"));
    CHECK(live_stable("binary"));
    CHECK(live_stable("bubble"));
    CHECK(live_stable("merge"));
    CHECK(live_stable("radix"));
    CHECK(!live_stable("shell"));
    CHECK(!live_stable("quick"));
    CHECK(!live_stable("select"));
    CHECK(!live_stable("heap"));
}

static void test_exam_letters(void) {
    CHECK_EQ(meter_2012(0), 'A');
    CHECK_EQ(meter_2015(0), 'C');
    CHECK_EQ(meter_2020(0), 'A');
    CHECK_EQ(meter_2025(0), 'D');
}

int main(void) {
    test_select_counts();
    test_heap_passes();
    test_merge_passes();
    test_radix_passes();
    test_merge_lists();
    test_shape_and_stable();
    test_exam_letters();
    return TEST_SUMMARY();
}
