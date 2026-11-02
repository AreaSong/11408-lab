#include "max.h"
#include "test.h"

static void test_normal(void){
    int a[] = {3,9,-2,7};
    int m =0;
    CHECK(max_find(a,4,&m));
    CHECK_EQ(m,9);
}

static void test_max_at_end(void) {
    int head[] = {8,1,2};
    int tail[] = {1,2,8};
    int m = 0;
    CHECK(max_find(head,3,&m));
    CHECK_EQ(m,8);
    CHECK(max_find(tail,3,&m));
    CHECK_EQ(m,8);
}

static void test_all_negative(void) {
    int a[] = {-5,-1,-8};
    int m =0;
    CHECK(max_find(a,3,&m));
    CHECK_EQ(m,-1);
}

static  void test_single_and_duplicates(void) {
    int one[] ={42};
    int same[] = {6,6,6,};
    int m=0;
    CHECK(max_find(one,1,&m));
    CHECK_EQ(m,42);
    CHECK(max_find(same,1,&m));
    CHECK_EQ(m,6);
}

static void test_empty(void) {
    int m =123;
    CHECK(!max_find(NULL,0,&m));
    CHECK_EQ(m,123);
}



int main(void) {
    test_normal();
    test_max_at_end();
    test_all_negative();
    test_single_and_duplicates();
    test_empty();


    return TEST_SUMMARY();
}
