#include "majority_element.h"
#include "seqlist.h"
#include "test.h"
#include "two_median.h"

static void expect_at(const SeqList *L, int i, int want) {
    int value = 12345;
    CHECK(seqlist_get(L, i, &value));
    CHECK_EQ(value, want);
}

static void test_empty_and_reject(void) {
    SeqList list;
    CHECK(seqlist_init(&list));
    CHECK_EQ(list.length, 0);
    CHECK_EQ(list.capacity, SEQLIST_INIT_CAP);
    CHECK_EQ(seqlist_locate(&list, 1), 0);

    int kept = 42;
    CHECK(!seqlist_get(&list, 1, &kept));
    CHECK_EQ(kept, 42);
    CHECK(!seqlist_delete(&list, 1, &kept, NULL));
    CHECK_EQ(kept, 42);
    CHECK(!seqlist_insert(&list, 0, 1, NULL));
    CHECK(!seqlist_insert(&list, 2, 1, NULL));
    CHECK_EQ(list.length, 0);
    seqlist_destroy(&list);
}

static void test_insert_delete_moves(void) {
    SeqList list;
    int moves = -1;
    CHECK(seqlist_init(&list));

    CHECK(seqlist_insert(&list, 1, 10, &moves));
    CHECK_EQ(moves, 0);
    CHECK(seqlist_insert(&list, 2, 30, &moves));
    CHECK_EQ(moves, 0);
    /* 位序 2：只有下标 1 上的 30 要后移一格 */
    CHECK(seqlist_insert(&list, 2, 20, &moves));
    CHECK_EQ(moves, 1);
    expect_at(&list, 1, 10);
    expect_at(&list, 2, 20);
    expect_at(&list, 3, 30);

    int value = 0;
    CHECK(seqlist_delete(&list, 1, &value, &moves));
    CHECK_EQ(value, 10);
    CHECK_EQ(moves, 2);
    CHECK(seqlist_delete(&list, 2, &value, &moves));
    CHECK_EQ(value, 30);
    CHECK_EQ(moves, 0);
    expect_at(&list, 1, 20);
    CHECK_EQ(list.length, 1);

    /* 非法位序失败时，表和输出参数都保持原样 */
    value = 7;
    moves = 9;
    CHECK(!seqlist_delete(&list, 2, &value, &moves));
    CHECK_EQ(value, 7);
    CHECK_EQ(moves, 9);
    CHECK_EQ(list.length, 1);
    seqlist_destroy(&list);
}

static void test_locate(void) {
    SeqList list;
    CHECK(seqlist_init(&list));
    CHECK(seqlist_insert(&list, 1, 5, NULL));
    CHECK(seqlist_insert(&list, 2, 5, NULL));
    CHECK(seqlist_insert(&list, 3, 7, NULL));
    /* 多个相同值时返回最靠前的位序，找不到是 0 而不是 -1 */
    CHECK_EQ(seqlist_locate(&list, 5), 1);
    CHECK_EQ(seqlist_locate(&list, 7), 3);
    CHECK_EQ(seqlist_locate(&list, 8), 0);
    seqlist_destroy(&list);
}

static void test_grow(void) {
    SeqList list;
    int moves = -1;
    CHECK(seqlist_init(&list));
    for (int i = 1; i <= SEQLIST_INIT_CAP; i++) {
        CHECK(seqlist_insert(&list, i, i, &moves));
        CHECK_EQ(moves, 0);
    }
    CHECK_EQ(list.capacity, SEQLIST_INIT_CAP);

    /* 插在表头：先加倍，再后移。移动次数只数后移，不含 realloc 的复制 */
    CHECK(seqlist_insert(&list, 1, 9, &moves));
    CHECK_EQ(moves, SEQLIST_INIT_CAP);
    CHECK_EQ(list.capacity, SEQLIST_INIT_CAP * 2);
    expect_at(&list, 1, 9);
    expect_at(&list, 2, 1);

    int cap = list.capacity;
    CHECK(seqlist_delete(&list, 1, NULL, NULL));
    CHECK_EQ(list.capacity, cap);

    CHECK(seqlist_grow(&list));
    CHECK_EQ(list.capacity, cap * 2);
    expect_at(&list, 1, 1);
    seqlist_destroy(&list);
}

/* 位置 1..n+1 各插入一次，总后移次数应是 0+1+…+n = n(n+1)/2 */
static void test_average_moves(void) {
    const int n = 3;
    SeqList list;
    CHECK(seqlist_init(&list));
    for (int i = 1; i <= n; i++) {
        CHECK(seqlist_insert(&list, i, i, NULL));
    }
    long long insert_total = 0;
    for (int i = 1; i <= n + 1; i++) {
        int moves = 0;
        CHECK(seqlist_insert(&list, i, -1, &moves));
        insert_total += moves;
        CHECK(seqlist_delete(&list, i, NULL, NULL));
    }
    long long delete_total = 0;
    for (int i = 1; i <= n; i++) {
        int moves = 0;
        int value = 0;
        CHECK(seqlist_delete(&list, i, &value, &moves));
        delete_total += moves;
        CHECK(seqlist_insert(&list, i, value, NULL));
    }
    CHECK_EQ(insert_total, (long long)n * (n + 1) / 2);
    CHECK_EQ(delete_total, (long long)n * (n - 1) / 2);
    CHECK_EQ(list.length, n);
    expect_at(&list, 1, 1);
    expect_at(&list, n, n);
    seqlist_destroy(&list);
}

/* 归并数出第 n 个，用来核对折半的结果，不参与卷面实现 */
static int median_brute(const int *a, const int *b, int n) {
    int i = 0;
    int j = 0;
    int cur = 0;
    for (int k = 0; k < n; k++) {
        if (i < n && (j == n || a[i] <= b[j])) {
            cur = a[i++];
        } else {
            cur = b[j++];
        }
    }
    return cur;
}

static void check_median(const int *a, const int *b, int n) {
    int got = two_median(a, b, n, NULL);
    CHECK_EQ(got, median_brute(a, b, n));
}

static void test_median(void) {
    const int a2011[] = {11, 13, 15, 17, 19};
    const int b2011[] = {2, 4, 6, 8, 20};
    CHECK_EQ(two_median(a2011, b2011, 5, NULL), 11);
    check_median(a2011, b2011, 5);

    const int one_a[] = {7};
    const int one_b[] = {3};
    CHECK_EQ(two_median(one_a, one_b, 1, NULL), 3);

    const int even_a[] = {1, 3, 5, 7};
    const int even_b[] = {2, 4, 6, 8};
    check_median(even_a, even_b, 4);

    const int low_a[] = {1, 2, 3};
    const int low_b[] = {4, 5, 6};
    check_median(low_a, low_b, 3);
    check_median(low_b, low_a, 3);

    const int eq_a[] = {1, 2, 3};
    const int eq_b[] = {0, 2, 9};
    CHECK_EQ(two_median(eq_a, eq_b, 3, NULL), 2);

    const int tie_a[] = {1, 1, 1, 1};
    const int tie_b[] = {1, 1, 1, 2};
    check_median(tie_a, tie_b, 4);

    const int pair_a[] = {1, 2};
    const int pair_b[] = {3, 4};
    check_median(pair_a, pair_b, 2);

    const int odd_a[] = {1, 3, 5, 7, 9, 11, 13};
    const int odd_b[] = {2, 4, 6, 8, 10, 12, 14};
    check_median(odd_a, odd_b, 7);

    const int all_a[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const int all_b[] = {9, 10, 11, 12, 13, 14, 15, 16};
    check_median(all_a, all_b, 8);
}

static void test_majority(void) {
    const int yes[] = {0, 5, 5, 3, 5, 7, 5, 5};
    const int no[] = {0, 5, 5, 3, 5, 1, 5, 7};
    CHECK_EQ(majority_element(yes, 8, NULL), 5);
    CHECK_EQ(majority_element(no, 8, NULL), -1);

    /* 0 可以是主元素；恰好一半不是 */
    const int zeros[] = {0, 0, 0, 1};
    const int half[] = {1, 1, 2, 2};
    CHECK_EQ(majority_element(zeros, 4, NULL), 0);
    CHECK_EQ(majority_element(half, 4, NULL), -1);

    const int one[] = {7};
    const int same[] = {4, 4, 4};
    const int tail[] = {1, 2, 2, 2};
    const int none[] = {1, 2, 3};
    const int two_zeros[] = {0, 0, 1};
    CHECK_EQ(majority_element(one, 1, NULL), 7);
    CHECK_EQ(majority_element(same, 3, NULL), 4);
    CHECK_EQ(majority_element(tail, 4, NULL), 2);
    CHECK_EQ(majority_element(none, 3, NULL), -1);
    CHECK_EQ(majority_element(two_zeros, 3, NULL), 0);
    CHECK_EQ(majority_element(NULL, 3, NULL), -1);
    CHECK_EQ(majority_element(one, 0, NULL), -1);
}

int main(void) {
    test_empty_and_reject();
    test_insert_delete_moves();
    test_locate();
    test_grow();
    test_average_moves();
    test_median();
    test_majority();
    return TEST_SUMMARY();
}
