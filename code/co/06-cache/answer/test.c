#include "cache.h"
#include "fields.h"
#include "test.h"
#include "walk.h"

#include <string.h>

static int open_ready(Cache **out, int word, int addr_bits, int block, int lines, int ways, int replace,
                      int write_back, int write_alloc) {
    Fields fields;
    if (fields_make(&fields, word, addr_bits, block, lines, ways) != 0) {
        return 1;
    }
    return cache_open(out, &fields, replace, write_back, write_alloc);
}

static void test_fields(void) {
    Fields f;
    uint64_t block = 0;
    uint64_t tag = 0;
    uint64_t offset = 0;
    int set = 0;
    Capacity cap;
    CHECK(fields_make(&f, 0, 28, 64, 8, 1) == 0);
    CHECK_EQ(f.tag_bits, 19);
    CHECK_EQ(f.index_bits, 3);
    CHECK_EQ(f.offset_bits, 6);
    CHECK(fields_make(&f, 0, 0, 32, 16, 2) == 0);
    CHECK(fields_locate(&f, 129, &block, &set, &tag, &offset) == 0);
    CHECK_EQ(block, 4);
    CHECK_EQ(set, 4);
    CHECK_EQ(offset, 1);
    CHECK(fields_make(&f, 0, 28, 64, 8, 1) == 0);
    CHECK(fields_locate(&f, 444, &block, &set, &tag, &offset) == 0);
    CHECK_EQ(set, 6);
    CHECK_EQ(offset, 60);
    CHECK(fields_locate(&f, 1348, &block, &set, &tag, &offset) == 0);
    CHECK_EQ(set, 5);
    CHECK_EQ(offset, 4);
    CHECK(capacity_make(&cap, &f, 0, REPLACE_LRU) == 0);
    CHECK_EQ(cap.total_bits, 4256);
    CHECK_EQ(cap.total_bits / 8, 532);
    CHECK_EQ(cap.dirty_bits, 0);
    CHECK_EQ(cap.replace_bits, 0);
    CHECK(fields_make(&f, 0, 28, 32, 16, 2) == 0);
    CHECK(capacity_make(&cap, &f, 1, REPLACE_LRU) == 0);
    CHECK_EQ(cap.tag_bits, 20);
    CHECK_EQ(cap.dirty_bits, 1);
    CHECK_EQ(cap.replace_bits, 1);
    CHECK_EQ(cap.total_bits, 4464);
    CHECK_EQ(cap.total_bits / 8, 558);
    CHECK(fields_make(&f, 0, 32, 64, 512, 8) == 0);
    CHECK_EQ(f.tag_bits, 20);
    CHECK_EQ(f.index_bits, 6);
    CHECK_EQ(f.offset_bits, 6);
    CHECK(capacity_make(&cap, &f, 0, REPLACE_LRU) == 0);
    CHECK_EQ(cap.replace_bits, 3);
    CHECK_EQ(cap.dirty_bits, 0);
}

static void test_2015(void) {
    Fields f;
    Capacity cap;
    CHECK(fields_make(&f, 0, 32, 16, 1024, 1) == 0);
    CHECK_EQ(f.tag_bits, 18);
    CHECK_EQ(f.offset_bits, 4);
    CHECK_EQ(f.index_bits, 10);
    CHECK(capacity_make(&cap, &f, 1, REPLACE_LRU) == 0);
    CHECK_EQ(cap.dirty_bits, 1);
    CHECK_EQ(cap.replace_bits, 0);
    CHECK_EQ(cap.total_bits, 151552);
    CHECK_EQ(cap.total_bits / 1024, 148);
}

static void test_rate_and_hits(void) {
    Cache *cache = NULL;
    CacheStat st;
    AccessInfo info;
    static const uint64_t seq[] = {0, 4, 8, 2, 0, 6, 8, 6, 4, 8};
    int i;
    CHECK(open_ready(&cache, 1, 8, 1, 4, 2, REPLACE_LRU, 0, 1) == 0);
    for (i = 0; i < 10; i++) {
        CHECK(cache_access(cache, 0, seq[i], &info) == 0);
    }
    cache_stat(cache, &st);
    CHECK_EQ(st.hits, 1);
    CHECK_EQ(st.misses, 9);
    cache_close(cache);
}

static void fill_walk(WalkIn *in, int rows, int cols, int elem, uint64_t base, int bits, int block, int lines,
                      int ways, int update) {
    memset(in, 0, sizeof(*in));
    in->rows = rows;
    in->cols = cols;
    in->elem_bytes = elem;
    in->base = base;
    in->addr_bits = bits;
    in->block_bytes = block;
    in->n_lines = lines;
    in->n_ways = ways;
    in->replace = REPLACE_LRU;
    in->write_alloc = 1;
    in->update = update;
}

static void test_row_col(void) {
    WalkIn in;
    WalkOut row;
    WalkOut col;
    fill_walk(&in, 256, 256, 4, 320, 28, 64, 8, 1, 0);
    CHECK(walk_run(&in, 0, &row) == 0);
    CHECK(walk_run(&in, 1, &col) == 0);
    CHECK_EQ(row.accesses, 65536);
    CHECK_EQ(row.hits, 61440);
    CHECK_EQ(row.misses, 4096);
    CHECK_EQ(col.hits, 0);
    CHECK_EQ(col.misses, 65536);
    fill_walk(&in, 4, 4, 4, 0, 16, 8, 2, 1, 0);
    CHECK(walk_run(&in, 0, &row) == 0);
    CHECK(walk_run(&in, 1, &col) == 0);
    CHECK_EQ(row.hits, 8);
    CHECK_EQ(row.misses, 8);
    CHECK_EQ(col.hits, 0);
    CHECK_EQ(col.misses, 16);
}

static void test_updates(void) {
    WalkIn in;
    WalkOut row;
    fill_walk(&in, 1, 1000, 4, 0, 32, 16, 64, 1, 1);
    CHECK(walk_run(&in, 0, &row) == 0);
    CHECK_EQ(row.accesses, 2000);
    CHECK_EQ(row.misses, 250);
    CHECK_EQ(row.hits, 1750);
    fill_walk(&in, 1, 1024, 4, 0x008000C0ull, 32, 64, 512, 8, 1);
    CHECK(walk_run(&in, 0, &row) == 0);
    CHECK_EQ(row.accesses, 2048);
    CHECK_EQ(row.misses, 64);
    CHECK_EQ(row.hits, 1984);
}

static void play(Cache *c, const char *ops, const uint64_t *addr, int n) {
    int i;
    for (i = 0; i < n; i++) {
        AccessInfo info;
        cache_access(c, ops[i] == 'W', addr[i], &info);
    }
}

static void test_replace(void) {
    Cache *lru = NULL;
    Cache *fifo = NULL;
    int valid = 0;
    int dirty = 0;
    uint64_t block = 0;
    static const uint64_t seq[] = {0, 1, 0, 2};
    CHECK(open_ready(&lru, 0, 8, 1, 2, 2, REPLACE_LRU, 1, 1) == 0);
    CHECK(open_ready(&fifo, 0, 8, 1, 2, 2, REPLACE_FIFO, 1, 1) == 0);
    play(lru, "RRRR", seq, 4);
    play(fifo, "RRRR", seq, 4);
    CHECK(cache_peek(lru, 0, 0, &valid, &dirty, &block) == 0);
    CHECK_EQ(valid, 1);
    CHECK_EQ(block, 0);
    CHECK(cache_peek(lru, 0, 1, &valid, &dirty, &block) == 0);
    CHECK_EQ(block, 2);
    CHECK(cache_peek(fifo, 0, 0, &valid, &dirty, &block) == 0);
    CHECK_EQ(block, 2);
    CHECK(cache_peek(fifo, 0, 1, &valid, &dirty, &block) == 0);
    CHECK_EQ(block, 1);
    cache_close(lru);
    cache_close(fifo);
}

static void expect_write(int write_back, int write_alloc, int hits, int misses, int writebacks, int mem_writes) {
    Cache *cache = NULL;
    CacheStat st;
    static const uint64_t seq[] = {0, 0, 0, 8};
    CHECK(open_ready(&cache, 0, 16, 4, 2, 1, REPLACE_LRU, write_back, write_alloc) == 0);
    play(cache, "WRWR", seq, 4);
    cache_stat(cache, &st);
    CHECK_EQ(st.hits, hits);
    CHECK_EQ(st.misses, misses);
    CHECK_EQ(st.writebacks, writebacks);
    CHECK_EQ(st.mem_writes, mem_writes);
    cache_close(cache);
}

static void test_write(void) {
    expect_write(1, 1, 2, 2, 1, 0);
    expect_write(0, 0, 1, 3, 0, 2);
    expect_write(1, 0, 1, 3, 1, 1);
    expect_write(0, 1, 2, 2, 0, 2);
}

int main(void) {
    test_fields();
    test_2015();
    test_rate_and_hits();
    test_row_col();
    test_updates();
    test_replace();
    test_write();
    return TEST_SUMMARY();
}
