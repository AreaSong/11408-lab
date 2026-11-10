#include "test.h"

#include <pthread.h>

#include "seqlist.h"


int main(void) {
    // SeqList L;
    // CHECK(seqlist_init(&L)==true);
    //
    // int moves = -1;
    // int out = -1;
    //
    // seqlist_insert(&L, 1, 10,NULL);
    // seqlist_insert(&L, 2, 20,NULL);
    // seqlist_insert(&L, 3, 30,NULL);
    // CHECK_EQ(L.length, 3);
    //
    // CHECK(seqlist_delete(&L,1,&out,&moves)==true);
    // CHECK_EQ(out, 10);
    // CHECK_EQ(moves, 2);
    // CHECK_EQ(L.length, 2);
    // CHECK_EQ(L.data[0], 20);
    // CHECK_EQ(L.data[1], 30);
    //
    // CHECK(seqlist_delete(&L,2,&out,&moves)==true);
    // CHECK_EQ(out, 30);
    // CHECK_EQ(moves, 0);
    // CHECK_EQ(L.length, 1);
    // CHECK_EQ(L.data[0], 20);
    //
    //
    // CHECK(seqlist_delete(&L,0,&out,&moves)==false);
    // CHECK(seqlist_delete(&L,2,&out,&moves)==false);
    //
    // seqlist_destroy(&L);

    // CHECK(seqlist_insert(&L,1,10,&moves)==true);
    // CHECK_EQ(moves, 0);
    // CHECK_EQ(L.length, 1);
    // CHECK_EQ(L.data[0], 10);
    //
    // CHECK(seqlist_insert(&L,2,30,&moves)==true);
    // CHECK_EQ(moves,0);
    // CHECK_EQ(L.data[1],30);
    //
    //
    // CHECK(seqlist_insert(&L,2,20,&moves)==true);
    // CHECK_EQ(moves, 1);
    // CHECK_EQ(L.data[0], 10);
    // CHECK_EQ(L.data[1], 20);
    // CHECK_EQ(L.data[2], 30);
    //
    // CHECK(seqlist_insert(&L,0,99,&moves)==false);
    // CHECK(seqlist_insert(&L,5,99,&moves)==false);
    //
    // CHECK(seqlist_insert(&L,4,40,&moves)==true);
    //
    // CHECK(seqlist_insert(&L,5,50,&moves)==true);
    // CHECK_EQ(L.capacity, 8);
    // CHECK_EQ(L.length, 5);
    //
    // seqlist_destroy(&L);


    // --------
    // CHECK(seqlist_init(&L)==true);
    // CHECK_EQ(L.length, 0);
    // CHECK_EQ(L.capacity, 4);
    // CHECK(L.data!=NULL);
    //
    // CHECK(seqlist_grow(&L)==true);
    // CHECK_EQ(L.capacity, 8);
    // CHECK(seqlist_grow(&L)==true);
    // CHECK_EQ(L.capacity, 16);
    //
    // seqlist_destroy(&L);
    // CHECK(L.data==NULL);
    // CHECK_EQ(L.length, 0);

    SeqList L;
    CHECK(seqlist_init(&L)==true);

    int moves = -1;
    int val = -1;

    CHECK(seqlist_insert(&L,1,10,NULL)==true);
    CHECK(seqlist_insert(&L,2,20,NULL)==true);
    CHECK(seqlist_insert(&L,3,30,NULL)==true);
    CHECK_EQ(L.length,3);

    CHECK(seqlist_get(&L,1,&val)==true);
    CHECK_EQ(val,10);
    CHECK(seqlist_get(&L,2,&val)==true);
    CHECK_EQ(val,20);
    CHECK(seqlist_get(&L,3,&val)==true);
    CHECK_EQ(val,30);

    CHECK(seqlist_get(&L,0,&val)==false);
    CHECK(seqlist_get(&L,4,&val)==false);


    CHECK_EQ(seqlist_locate(&L,10),1);
    CHECK_EQ(seqlist_locate(&L,20),2);
    CHECK_EQ(seqlist_locate(&L,30),3);
    CHECK_EQ(seqlist_locate(&L,999),0);

    CHECK(seqlist_delete(&L,1,&val,&moves)==true);
    CHECK_EQ(val,10);
    CHECK_EQ(moves,2);
    CHECK_EQ(L.length,2);

    CHECK_EQ(seqlist_locate(&L,20),1);
    CHECK_EQ(seqlist_locate(&L,30),2);

    CHECK(seqlist_insert(&L,3,40,NULL)==true);
    CHECK(seqlist_insert(&L,4,50,NULL)==true);
    CHECK(seqlist_insert(&L,5,60,NULL)==true);
    CHECK_EQ(L.capacity,8);
    CHECK_EQ(L.length,5);

    seqlist_destroy(&L);
    CHECK(L.data==NULL);

    return TEST_SUMMARY();
}
