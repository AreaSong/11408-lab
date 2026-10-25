#include "max.h"
#include "test.h"

static void test_normal(void) {
    int a[] = {3, 9, -2, 7};
    int m = 0;
    CHECK(max_find(a, 4, &m));
    CHECK_EQ(m, 9);
}

/* 边界一：最大值在第一个或最后一个位置，循环起止下标写错就会漏掉 */
static void test_max_at_ends(void) {
    int head[] = {8, 1, 2};
    int tail[] = {1, 2, 8};
    int m = 0;
    CHECK(max_find(head, 3, &m));
    CHECK_EQ(m, 8);
    CHECK(max_find(tail, 3, &m));
    CHECK_EQ(m, 8);
}

/* 边界二：全是负数，专门抓"初值写成 0"的错误 */
static void test_all_negative(void) {
    int a[] = {-5, -1, -8};
    int m = 0;
    CHECK(max_find(a, 3, &m));
    CHECK_EQ(m, -1);
}

static void test_single_and_duplicates(void) {
    int one[] = {42};
    int same[] = {6, 6, 6};
    int m = 0;
    CHECK(max_find(one, 1, &m));
    CHECK_EQ(m, 42);
    CHECK(max_find(same, 3, &m));
    CHECK_EQ(m, 6);
}

/* 边界三：空表返回 false，且不能改动调用方的变量 */
static void test_empty(void) {
    int m = 123;
    CHECK(!max_find(NULL, 0, &m));
    CHECK_EQ(m, 123);
}

int main(void) {
    test_normal();
    test_max_at_ends();
    test_all_negative();
    test_single_and_duplicates();
    test_empty();
    return TEST_SUMMARY();
}
