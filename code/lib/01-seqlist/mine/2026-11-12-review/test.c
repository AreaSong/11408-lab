#include "test.h"
#include "seqlist.h"
#include "majority_element.h"

int main(void) {
    SeqList L;
    seqlist_init(&L);
    CHECK_EQ(L.length, 0);
    CHECK_EQ(L.capacity, 4);

    CHECK_EQ(seqlist_insert(&L,1,10), 1);
    CHECK_EQ(seqlist_insert(&L,2,30), 1);
    CHECK_EQ(seqlist_insert(&L,2,20), 1);

    CHECK_EQ(L.data[1], 20);
    CHECK_EQ(L.length, 3);

    CHECK_EQ(seqlist_insert(&L,4,40), 1);
    CHECK_EQ(seqlist_insert(&L,5,50), 1);
    CHECK_EQ(L.capacity, 8);
    CHECK_EQ(L.data[4], 50);

    CHECK_EQ(seqlist_insert(&L,10,99), 0);

    CHECK_EQ(seqlist_delete(&L,2), 1);
    CHECK_EQ(L.length, 4);
    CHECK_EQ(seqlist_get(&L, 1), 10);
    CHECK_EQ(seqlist_get(&L,2), 30);
    CHECK_EQ(seqlist_find(&L,40), 3);
    CHECK_EQ(seqlist_find(&L,999), 0);
    CHECK_EQ(seqlist_delete(&L,10), 0);

    int a1[] = {0, 5, 5, 3, 5, 7, 5, 5};
    CHECK_EQ(majority_element(a1,8),5);

    int a2[] = {0, 5, 5, 3, 5, 1, 5, 7};
    CHECK_EQ(majority_element(a2, 8), -1);

    int a3[] = {2, 6, 3, 6, 4, 6, 4, 6, 4, 6, 6, 6};
    CHECK_EQ(majority_element(a3, 12), 6);

    return TEST_SUMMARY();
}
