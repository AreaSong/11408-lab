#include "buddy.h"
#include "exam.h"
#include "fixed.h"
#include "reloc.h"
#include "space.h"
#include "test.h"

static const long long kFive[] = {0, 100, 1000, 500, 2000, 200, 3000, 300, 4000, 600};

static void begin_holes(Space *s, Algo algo, const long long *pairs, int n) {
    int i;
    space_begin(s, algo, 1);
    for (i = 0; i < n; i++) {
        CHECK_EQ(space_add_hole(s, pairs[2 * i], pairs[2 * i + 1]), 0);
    }
}

static void test_reloc(void) {
    int ok = 0;
    long long physical = 0;
    CHECK_EQ(reloc_logical(3000, 500, 400, &ok, &physical), 0);
    CHECK_EQ(ok, 1);
    CHECK_EQ(physical, 3400);
    CHECK_EQ(reloc_logical(3000, 500, 499, &ok, &physical), 0);
    CHECK_EQ(physical, 3499);
    CHECK_EQ(reloc_logical(3000, 500, 500, &ok, &physical), 0);
    CHECK_EQ(ok, 0);
    CHECK_EQ(reloc_logical(3000, 500, 600, &ok, &physical), 0);
    CHECK_EQ(ok, 0);
    CHECK_EQ(reloc_logical(3000, 500, 0, &ok, &physical), 0);
    CHECK_EQ(ok, 1);
    CHECK_EQ(physical, 3000);
    CHECK_EQ(reloc_logical(100, 0, 0, &ok, &physical), 0);
    CHECK_EQ(ok, 0);
    CHECK_EQ(reloc_bounds(3000, 3499, 3000, &ok), 0);
    CHECK_EQ(ok, 1);
    CHECK_EQ(reloc_bounds(3000, 3499, 3499, &ok), 0);
    CHECK_EQ(ok, 1);
    CHECK_EQ(reloc_bounds(3000, 3499, 3500, &ok), 0);
    CHECK_EQ(ok, 0);
    CHECK_EQ(reloc_bounds(5, 4, 5, &ok), -1);
}

static void test_2017(void) {
    Space s;
    FreeInfo info;
    Hole holes[4];
    begin_holes(&s, ALGO_BF, (const long long[]){20, 40, 500, 80, 1000, 100, 200, 200}, 4);
    CHECK_EQ(space_release(&s, 60, 140, &info), 0);
    CHECK_EQ(info.merge, 3);
    CHECK_EQ(info.into, 20);
    CHECK_EQ(info.into_size, 380);
    CHECK_EQ(space_chain(&s, holes, 4), 3);
    CHECK_EQ(holes[0].start, 500);
    CHECK_EQ(holes[0].size, 80);
    CHECK_EQ(holes[1].start, 1000);
    CHECK_EQ(holes[1].size, 100);
    CHECK_EQ(holes[2].start, 20);
    CHECK_EQ(holes[2].size, 380);
}

static void test_2010(void) {
    Space s;
    AllocInfo alloc;
    FreeInfo info;
    Hole holes[4];
    long long sum = 0;
    long long largest = 0;
    CHECK_EQ(space_memory(&s, ALGO_BF, 55, 1), 0);
    CHECK_EQ(space_alloc(&s, "A", 15, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "B", 30, &alloc), 0);
    CHECK_EQ(space_free_name(&s, "A", &info), 0);
    CHECK_EQ(info.merge, 0);
    CHECK_EQ(space_alloc(&s, "C", 8, &alloc), 0);
    CHECK_EQ(alloc.at, 45);
    CHECK_EQ(alloc.left, 2);
    CHECK_EQ(space_alloc(&s, "D", 6, &alloc), 0);
    CHECK_EQ(alloc.at, 0);
    CHECK_EQ(alloc.left, 9);
    space_stat(&s, &sum, &largest);
    CHECK_EQ(largest, 9);
    CHECK_EQ(sum, 11);
    CHECK_EQ(space_chain(&s, holes, 4), 2);
    CHECK_EQ(holes[0].start, 53);
    CHECK_EQ(holes[0].size, 2);
    CHECK_EQ(holes[1].start, 6);
    CHECK_EQ(holes[1].size, 9);
}

static void test_merge(void) {
    Space s;
    FreeInfo info;
    Hole holes[4];
    begin_holes(&s, ALGO_FF, (const long long[]){0, 10}, 1);
    CHECK_EQ(space_release(&s, 10, 10, &info), 0);
    CHECK_EQ(info.merge, 1);
    CHECK_EQ(info.into, 0);
    CHECK_EQ(info.into_size, 20);
    begin_holes(&s, ALGO_FF, (const long long[]){20, 10}, 1);
    CHECK_EQ(space_release(&s, 10, 10, &info), 0);
    CHECK_EQ(info.merge, 2);
    CHECK_EQ(info.into, 10);
    CHECK_EQ(info.into_size, 20);
    begin_holes(&s, ALGO_FF, (const long long[]){0, 10, 20, 10}, 2);
    CHECK_EQ(space_release(&s, 10, 10, &info), 0);
    CHECK_EQ(info.merge, 3);
    CHECK_EQ(info.into, 0);
    CHECK_EQ(info.into_size, 30);
    begin_holes(&s, ALGO_FF, (const long long[]){0, 10, 30, 10}, 2);
    CHECK_EQ(space_release(&s, 15, 10, &info), 0);
    CHECK_EQ(info.merge, 0);
    CHECK_EQ(space_chain(&s, holes, 4), 3);
}

static void test_ff_bf_wf(void) {
    Space s;
    AllocInfo alloc;
    begin_holes(&s, ALGO_FF, kFive, 5);
    CHECK_EQ(space_alloc(&s, "A", 212, &alloc), 0);
    CHECK_EQ(alloc.at, 1000);
    CHECK_EQ(space_alloc(&s, "B", 117, &alloc), 0);
    CHECK_EQ(alloc.at, 1212);
    CHECK_EQ(space_alloc(&s, "C", 526, &alloc), 0);
    CHECK_EQ(alloc.ok, 1);
    CHECK_EQ(alloc.at, 4000);
    begin_holes(&s, ALGO_BF, kFive, 5);
    CHECK_EQ(space_alloc(&s, "A", 212, &alloc), 0);
    CHECK_EQ(alloc.at, 3000);
    CHECK_EQ(space_alloc(&s, "B", 117, &alloc), 0);
    CHECK_EQ(alloc.at, 2000);
    CHECK_EQ(space_alloc(&s, "C", 526, &alloc), 0);
    CHECK_EQ(alloc.at, 4000);
    begin_holes(&s, ALGO_WF, kFive, 5);
    CHECK_EQ(space_alloc(&s, "A", 212, &alloc), 0);
    CHECK_EQ(alloc.at, 4000);
    CHECK_EQ(alloc.left, 388);
    CHECK_EQ(space_alloc(&s, "B", 117, &alloc), 0);
    CHECK_EQ(alloc.at, 1000);
    CHECK_EQ(alloc.left, 383);
    CHECK_EQ(space_alloc(&s, "C", 526, &alloc), 0);
    CHECK_EQ(alloc.ok, 0);
    CHECK_EQ(alloc.largest, 388);
    CHECK_EQ(alloc.external, 1371);
}

static void test_keep(void) {
    Space s;
    AllocInfo alloc;
    const long long pairs[] = {0, 120, 1000, 200, 2000, 120};
    begin_holes(&s, ALGO_FF, pairs, 3);
    CHECK_EQ(space_alloc(&s, "A", 100, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "B", 100, &alloc), 0);
    CHECK_EQ(alloc.at, 1000);
    CHECK_EQ(space_alloc(&s, "C", 150, &alloc), 0);
    CHECK_EQ(alloc.ok, 0);
    CHECK_EQ(alloc.largest, 120);
    begin_holes(&s, ALGO_BF, pairs, 3);
    CHECK_EQ(space_alloc(&s, "A", 100, &alloc), 0);
    CHECK_EQ(alloc.at, 0);
    CHECK_EQ(space_alloc(&s, "B", 100, &alloc), 0);
    CHECK_EQ(alloc.at, 2000);
    CHECK_EQ(space_alloc(&s, "C", 150, &alloc), 0);
    CHECK_EQ(alloc.ok, 1);
    CHECK_EQ(alloc.at, 1000);
}

static void test_nf(void) {
    Space s;
    AllocInfo alloc;
    FreeInfo info;
    long long cursor = 0;
    const long long pairs[] = {0, 30, 1000, 100, 2000, 50};
    begin_holes(&s, ALGO_NF, pairs, 3);
    CHECK_EQ(space_alloc(&s, "A", 40, &alloc), 0);
    CHECK_EQ(alloc.at, 1000);
    CHECK_EQ(space_alloc(&s, "B", 20, &alloc), 0);
    CHECK_EQ(alloc.at, 1040);
    begin_holes(&s, ALGO_FF, pairs, 3);
    CHECK_EQ(space_alloc(&s, "A", 40, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "B", 20, &alloc), 0);
    CHECK_EQ(alloc.at, 0);
    begin_holes(&s, ALGO_NF, (const long long[]){0, 100, 500, 10}, 2);
    CHECK_EQ(space_alloc(&s, "C", 100, &alloc), 0);
    CHECK_EQ(alloc.left, 0);
    CHECK_EQ(space_free_name(&s, "C", &info), 0);
    CHECK_EQ(space_alloc(&s, "D", 50, &alloc), 0);
    CHECK_EQ(alloc.at, 0);
    CHECK_EQ(space_cursor(&s, &cursor), 0);
    CHECK_EQ(cursor, 50);
}

static void test_compact(void) {
    Space s;
    AllocInfo alloc;
    FreeInfo info;
    Move moves[4];
    Job jobs[4];
    int nmove = 0;
    long long free_at = 0;
    long long free_size = 0;
    CHECK_EQ(space_memory(&s, ALGO_FF, 100, 1), 0);
    CHECK_EQ(space_alloc(&s, "A", 20, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "B", 30, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "C", 10, &alloc), 0);
    CHECK_EQ(space_free_name(&s, "B", &info), 0);
    space_set_bind(&s, 0);
    CHECK_EQ(space_compact(&s, moves, 4, &nmove, &free_at, &free_size), -2);
    CHECK_EQ(space_jobs(&s, jobs, 4), 2);
    CHECK_EQ(jobs[1].start, 50);
    space_set_bind(&s, 1);
    CHECK_EQ(space_compact(&s, moves, 4, &nmove, &free_at, &free_size), 0);
    CHECK_EQ(nmove, 2);
    CHECK_EQ(moves[0].new_at, 0);
    CHECK_EQ(moves[1].old_at, 50);
    CHECK_EQ(moves[1].new_at, 20);
    CHECK_EQ(free_at, 30);
    CHECK_EQ(free_size, 70);
}

static void test_fixed_single(void) {
    Fixed fixed;
    FixedPut put;
    Single single;
    SinglePut one;
    fixed_begin(&fixed);
    CHECK_EQ(fixed_slot(&fixed, 0, 50), 0);
    CHECK_EQ(fixed_slot(&fixed, 50, 50), 0);
    CHECK_EQ(fixed_put(&fixed, "A", 20, &put), 0);
    CHECK_EQ(put.internal, 30);
    CHECK_EQ(put.sum, 30);
    CHECK_EQ(fixed_put(&fixed, "B", 20, &put), 0);
    CHECK_EQ(put.sum, 60);
    CHECK_EQ(fixed_put(&fixed, "C", 60, &put), 0);
    CHECK_EQ(put.ok, 0);
    CHECK_EQ(put.sum, 60);
    CHECK_EQ(single_begin(&single, 100), 0);
    CHECK_EQ(single_put(&single, "A", 20, &one), 0);
    CHECK_EQ(one.internal, 80);
    CHECK_EQ(single_put(&single, "B", 10, &one), 0);
    CHECK_EQ(one.busy, 1);
    CHECK_EQ(one.ok, 0);
}

static void test_buddy(void) {
    Buddy buddy;
    BuddyPut put;
    BuddyBlk blocks[4];
    Space s;
    AllocInfo alloc;
    FreeInfo info;
    Hole holes[4];
    int merges = 0;
    CHECK_EQ(buddy_open(&buddy, 12), -1);
    CHECK_EQ(buddy_open(&buddy, 16), 0);
    CHECK_EQ(buddy_alloc(&buddy, "A", 3, &put), 0);
    CHECK_EQ(put.size, 4);
    CHECK_EQ(put.at, 0);
    CHECK_EQ(buddy_free(&buddy, "A", &merges), 0);
    CHECK_EQ(merges, 2);
    CHECK_EQ(buddy_collect(&buddy, 1, blocks, 4), 1);
    CHECK_EQ(blocks[0].size, 16);
    CHECK_EQ(buddy_open(&buddy, 16), 0);
    CHECK_EQ(buddy_alloc(&buddy, "A", 4, &put), 0);
    CHECK_EQ(buddy_alloc(&buddy, "B", 4, &put), 0);
    CHECK_EQ(buddy_alloc(&buddy, "C", 4, &put), 0);
    CHECK_EQ(buddy_alloc(&buddy, "D", 4, &put), 0);
    CHECK_EQ(buddy_free(&buddy, "B", &merges), 0);
    CHECK_EQ(merges, 0);
    CHECK_EQ(buddy_free(&buddy, "C", &merges), 0);
    CHECK_EQ(merges, 0);
    CHECK_EQ(buddy_collect(&buddy, 1, blocks, 4), 2);
    CHECK_EQ(blocks[0].start, 4);
    CHECK_EQ(blocks[1].start, 8);
    CHECK_EQ(blocks[0].size, 4);
    CHECK_EQ(blocks[1].size, 4);
    CHECK_EQ(space_memory(&s, ALGO_FF, 16, 1), 0);
    CHECK_EQ(space_alloc(&s, "A", 4, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "B", 4, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "C", 4, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "D", 4, &alloc), 0);
    CHECK_EQ(space_free_name(&s, "B", &info), 0);
    CHECK_EQ(space_free_name(&s, "C", &info), 0);
    CHECK_EQ(info.merge, 1);
    CHECK_EQ(info.into, 4);
    CHECK_EQ(info.into_size, 8);
    CHECK_EQ(space_chain(&s, holes, 4), 1);
}

static void test_exam(void) {
    char letter = '?';
    const char *note = 0;
    CHECK_EQ(exam_letter("2009-26", &letter, &note), 0);
    CHECK_EQ(letter, 'A');
    CHECK(note == 0);
    CHECK_EQ(exam_letter("2011-30", &letter, &note), 0);
    CHECK_EQ(letter, 'B');
    CHECK_STR(note, "compile-module");
    CHECK_EQ(exam_letter("2019-32", &letter, &note), 0);
    CHECK_EQ(letter, 'C');
    CHECK_EQ(exam_letter("2024-27", &letter, &note), 0);
    CHECK_EQ(letter, 'A');
    CHECK_EQ(exam_letter("1999-1", &letter, &note), -1);
}

static void test_edge(void) {
    Space s;
    AllocInfo alloc;
    Hole holes[2];
    begin_holes(&s, ALGO_FF, (const long long[]){0, 10}, 1);
    CHECK_EQ(space_add_hole(&s, 5, 10), -1);
    CHECK_EQ(space_add_hole(&s, 10, 10), 0);
    CHECK_EQ(space_chain(&s, holes, 2), 1);
    CHECK_EQ(holes[0].size, 20);
    CHECK_EQ(space_memory(&s, ALGO_FF, 20, 1), 0);
    CHECK_EQ(space_alloc(&s, "A", 4, &alloc), 0);
    CHECK_EQ(space_alloc(&s, "A", 4, &alloc), -3);
    CHECK_EQ(space_release(&s, 0, 4, &(FreeInfo){0}), -4);
}

int main(void) {
    test_reloc();
    test_2017();
    test_2010();
    test_merge();
    test_ff_bf_wf();
    test_keep();
    test_nf();
    test_compact();
    test_fixed_single();
    test_buddy();
    test_exam();
    test_edge();
    return TEST_SUMMARY();
}
