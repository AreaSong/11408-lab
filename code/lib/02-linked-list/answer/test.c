#include "clist.h"
#include "dlist.h"
#include "kth.h"
#include "rearrange.h"
#include "slist.h"
#include "staticlist.h"
#include "test.h"

static void expect_slist(const SList *L, const int *a, int n) {
    CHECK_EQ(slist_length(L), n);
    CHECK(slist_empty(L) == (n == 0));
    if (n == 0) {
        CHECK(L->tail == L->head);
        CHECK(L->head->next == NULL);
    } else {
        CHECK_EQ(L->tail->data, a[n - 1]);
        CHECK(L->tail->next == NULL);
    }
    for (int i = 0; i < n; i++) {
        int value = 0;
        CHECK(slist_get(L, i + 1, &value));
        CHECK_EQ(value, a[i]);
    }
}

static void test_build_and_reject(void) {
    SList list;
    CHECK(slist_init(&list));
    CHECK(slist_empty(&list));
    CHECK_EQ(slist_locate(&list, 1), 0);

    int kept = 42;
    CHECK(!slist_get(&list, 1, &kept));
    CHECK_EQ(kept, 42);
    CHECK(!slist_delete(&list, 1, &kept));
    CHECK_EQ(kept, 42);
    CHECK(!slist_insert(&list, 0, 1));
    CHECK(!slist_insert(&list, 2, 1));
    CHECK_EQ(slist_length(&list), 0);

    /* 头插得到逆序，尾插得到原序。空表时 tail 就是头结点，第一次插入后要跟上。 */
    CHECK(slist_head_insert(&list, 1));
    CHECK(slist_head_insert(&list, 2));
    CHECK(slist_head_insert(&list, 3));
    const int head_order[] = {3, 2, 1};
    expect_slist(&list, head_order, 3);

    CHECK(slist_tail_insert(&list, 4));
    const int with_tail[] = {3, 2, 1, 4};
    expect_slist(&list, with_tail, 4);

    CHECK(slist_insert(&list, 2, 9));
    const int inserted[] = {3, 9, 2, 1, 4};
    expect_slist(&list, inserted, 5);
    CHECK_EQ(slist_locate(&list, 9), 2);
    CHECK_EQ(slist_locate(&list, 3), 1);
    CHECK_EQ(slist_locate(&list, 8), 0);

    int value = 0;
    CHECK(slist_delete(&list, 5, &value));
    CHECK_EQ(value, 4);
    const int after_del[] = {3, 9, 2, 1};
    expect_slist(&list, after_del, 4);

    value = 7;
    CHECK(!slist_delete(&list, 5, &value));
    CHECK_EQ(value, 7);
    CHECK(!slist_insert(&list, 6, 1));
    expect_slist(&list, after_del, 4);
    slist_destroy(&list);
}

static void test_reverse_then_tail(void) {
    SList list;
    CHECK(slist_init(&list));
    slist_reverse(&list);
    CHECK(slist_empty(&list));

    CHECK(slist_tail_insert(&list, 7));
    slist_reverse(&list);
    const int one[] = {7};
    expect_slist(&list, one, 1);

    CHECK(slist_tail_insert(&list, 8));
    CHECK(slist_tail_insert(&list, 9));
    slist_reverse(&list);
    const int rev[] = {9, 8, 7};
    expect_slist(&list, rev, 3);
    slist_reverse(&list);
    const int back[] = {7, 8, 9};
    expect_slist(&list, back, 3);

    /* 逆置之后 tail 必须改到新表尾，否则再尾插会接到旧表尾后面。 */
    CHECK(slist_tail_insert(&list, 1));
    const int appended[] = {7, 8, 9, 1};
    expect_slist(&list, appended, 4);
    slist_destroy(&list);
}

static void expect_circle(const CList *L, const int *a, int n) {
    CHECK(clist_ring_ok(L));
    CHECK_EQ(clist_length(L), n);
    CHECK(clist_empty(L) == (n == 0));
    CHECK(L->rear->next == L->head);
    if (n == 0) {
        CHECK(L->rear == L->head);
    } else {
        CHECK_EQ(L->rear->data, a[n - 1]);
    }
    for (int i = 0; i < n; i++) {
        int value = 0;
        CHECK(clist_get(L, i + 1, &value));
        CHECK_EQ(value, a[i]);
    }
}

static void test_circle(void) {
    CList list;
    CHECK(clist_init(&list));
    CHECK(list.head->next == list.head);
    const int none[] = {0};
    expect_circle(&list, none, 0);

    CHECK(clist_head_insert(&list, 1));
    CHECK(clist_head_insert(&list, 2));
    CHECK(clist_tail_insert(&list, 3));
    const int built[] = {2, 1, 3};
    expect_circle(&list, built, 3);
    CHECK_EQ(clist_locate(&list, 1), 2);
    CHECK_EQ(clist_locate(&list, 9), 0);

    CHECK(clist_insert(&list, 2, 8));
    const int mid[] = {2, 8, 1, 3};
    expect_circle(&list, mid, 4);

    int value = 0;
    CHECK(clist_delete(&list, 4, &value));
    CHECK_EQ(value, 3);
    const int no_tail[] = {2, 8, 1};
    expect_circle(&list, no_tail, 3);
    CHECK(clist_delete(&list, 1, &value));
    CHECK(clist_delete(&list, 1, &value));
    CHECK(clist_delete(&list, 1, &value));
    expect_circle(&list, none, 0);
    CHECK(!clist_delete(&list, 1, &value));
    clist_destroy(&list);
}

static void expect_dlist(const DList *L, const int *a, int n) {
    CHECK(dlist_links_ok(L));
    CHECK_EQ(dlist_length(L), n);
    CHECK(dlist_empty(L) == (n == 0));
    for (int i = 0; i < n; i++) {
        int value = 0;
        CHECK(dlist_get(L, i + 1, &value));
        CHECK_EQ(value, a[i]);
    }
}

static void test_cdouble(void) {
    DList list;
    CHECK(dlist_init(&list, true));
    CHECK(list.head->next == list.head);
    CHECK(list.head->prior == list.head);
    const int none[] = {0};
    expect_dlist(&list, none, 0);

    CHECK(dlist_insert(&list, 1, 10));
    CHECK(dlist_insert(&list, 2, 30));
    CHECK(dlist_insert(&list, 2, 20));
    const int seq[] = {10, 20, 30};
    expect_dlist(&list, seq, 3);
    CHECK(list.head->prior->data == 30);
    CHECK(list.head->next->data == 10);

    int value = 0;
    CHECK(dlist_delete(&list, 2, &value));
    CHECK_EQ(value, 20);
    const int left[] = {10, 30};
    expect_dlist(&list, left, 2);
    CHECK(dlist_delete(&list, 1, NULL));
    CHECK(dlist_delete(&list, 1, NULL));
    expect_dlist(&list, none, 0);
    CHECK(list.head->next == list.head);
    CHECK(list.head->prior == list.head);
    CHECK(!dlist_insert(&list, 0, 1));
    dlist_destroy(&list);
}

static void test_ndouble(void) {
    DList list;
    CHECK(dlist_init(&list, false));
    CHECK(list.head->next == NULL);
    CHECK(list.head->prior == NULL);

    CHECK(dlist_insert(&list, 1, 10));
    CHECK(dlist_insert(&list, 2, 30));
    CHECK(dlist_insert(&list, 2, 20));
    const int seq[] = {10, 20, 30};
    expect_dlist(&list, seq, 3);
    CHECK(list.head->prior == NULL);
    CHECK(list.head->next->prior == list.head);

    int value = 0;
    CHECK(dlist_delete(&list, 3, &value));
    CHECK_EQ(value, 30);
    const int no_tail[] = {10, 20};
    expect_dlist(&list, no_tail, 2);
    CHECK(dlist_delete(&list, 1, NULL));
    CHECK(dlist_delete(&list, 1, NULL));
    CHECK(list.head->next == NULL);
    CHECK(list.head->prior == NULL);
    CHECK_EQ(dlist_locate(&list, 10), 0);
    dlist_destroy(&list);
}

static void test_static_reuse_and_full(void) {
    StaticList list;
    staticlist_init(&list);
    CHECK(staticlist_empty(&list));
    CHECK_EQ(list.node[0].next, STATIC_NULL);
    CHECK_EQ(list.spare, 1);

    CHECK_EQ(staticlist_insert(&list, 1, 10), STATIC_OK);
    CHECK_EQ(staticlist_insert(&list, 2, 20), STATIC_OK);
    CHECK_EQ(staticlist_insert(&list, 3, 30), STATIC_OK);
    CHECK_EQ(list.node[1].data, 10);
    CHECK_EQ(list.node[1].next, 2);
    CHECK_EQ(list.node[2].next, 3);
    CHECK_EQ(list.node[3].next, STATIC_NULL);

    int value = 0;
    CHECK(staticlist_delete(&list, 2, &value));
    CHECK_EQ(value, 20);
    /* 删掉的下标 2 头插回备用链表，格子里的 20 还在，只是不再属于逻辑表。 */
    CHECK_EQ(list.spare, 2);
    CHECK_EQ(list.node[2].data, 20);
    CHECK_EQ(list.node[2].next, 4);
    CHECK_EQ(list.node[1].next, 3);

    CHECK_EQ(staticlist_insert(&list, 2, 99), STATIC_OK);
    CHECK_EQ(list.node[2].data, 99);
    CHECK_EQ(list.node[1].next, 2);
    CHECK_EQ(list.node[2].next, 3);
    CHECK_EQ(staticlist_locate(&list, 99), 2);
    CHECK_EQ(staticlist_locate(&list, 20), 0);

    StaticList full;
    staticlist_init(&full);
    for (int i = 1; i <= STATIC_CAP - 1; i++) {
        CHECK_EQ(staticlist_insert(&full, i, i), STATIC_OK);
    }
    CHECK_EQ(full.spare, STATIC_NULL);
    CHECK_EQ(staticlist_insert(&full, 1, 100), STATIC_FULL);
    CHECK_EQ(staticlist_length(&full), STATIC_CAP - 1);
    CHECK_EQ(staticlist_insert(&full, 0, 1), STATIC_RANGE);
    CHECK(!staticlist_delete(&full, 0, &value));
    CHECK(staticlist_get(&full, 1, &value));
    CHECK_EQ(value, 1);
    CHECK(!staticlist_get(&full, STATIC_CAP, &value));
}

static void check_kth(const int *a, int n, int k) {
    SList list;
    CHECK(slist_init(&list));
    for (int i = 0; i < n; i++) {
        CHECK(slist_tail_insert(&list, a[i]));
    }
    int value = 12345;
    int found = list_kth(list.head, k, &value, NULL);
    if (k < 1 || k > n) {
        CHECK_EQ(found, 0);
        CHECK_EQ(value, 12345);
    } else {
        CHECK_EQ(found, 1);
        CHECK_EQ(value, a[n - k]);
    }
    expect_slist(&list, a, n);
    slist_destroy(&list);
}

static void test_kth(void) {
    const int a[] = {1, 2, 3, 4, 5};
    for (int k = 0; k <= 6; k++) {
        check_kth(a, 5, k);
    }
    const int one[] = {7};
    check_kth(one, 1, 1);
    check_kth(one, 1, 2);
    check_kth(a, 0, 1);
}

static void check_rearrange(const int *a, int n) {
    SList list;
    CHECK(slist_init(&list));
    for (int i = 0; i < n; i++) {
        CHECK(slist_tail_insert(&list, a[i]));
    }
    list_rearrange(list.head, NULL);
    int left = 0;
    int right = n - 1;
    for (int i = 0; i < n; i++) {
        int value = 0;
        CHECK(slist_get(&list, i + 1, &value));
        if (i % 2 == 0) {
            CHECK_EQ(value, a[left]);
            left++;
        } else {
            CHECK_EQ(value, a[right]);
            right--;
        }
    }
    CHECK_EQ(slist_length(&list), n);
    slist_destroy(&list);
}

static void test_rearrange(void) {
    const int even[] = {1, 2, 3, 4, 5, 6};
    const int odd[] = {1, 2, 3, 4, 5};
    const int two[] = {1, 2};
    const int one[] = {7};
    check_rearrange(even, 6);
    check_rearrange(odd, 5);
    check_rearrange(two, 2);
    check_rearrange(one, 1);
    check_rearrange(one, 0);
    const int seven[] = {1, 2, 3, 4, 5, 6, 7};
    check_rearrange(seven, 7);
}

int main(void) {
    test_build_and_reject();
    test_reverse_then_tail();
    test_circle();
    test_cdouble();
    test_ndouble();
    test_static_reuse_and_full();
    test_kth();
    test_rearrange();
    return TEST_SUMMARY();
}
