#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_exec.h"
#include "cpu/cpu_pipe.h"
#include "mem/mem_bank.h"
#include "mem/mem_cache.h"
#include "mem/mem_chip.h"
#include "test.h"

#include <string.h>

static void test_chip_and_bank(void) {
    MemChipReq req = {8192, 8, 2048, 4};
    MemChipPlan plan;
    MemChipAt at;
    uint32_t addrs[] = {8005, 8006, 8007, 8008, 8001, 8002, 8003, 8004, 8000};
    int left[8];
    int right[8];
    int pairs = 0;
    CHECK(mem_chip_plan(&req, &plan) == 0);
    CHECK_EQ(plan.chips, 8);
    CHECK_EQ(plan.groups, 4);
    CHECK_EQ(plan.per_group, 2);
    CHECK_EQ(plan.mode, MEM_EXT_BOTH);
    CHECK_EQ(plan.chip_addr_bits, 11);
    CHECK_EQ(plan.select_bits, 2);
    CHECK(mem_chip_locate(8192, 2048, 0xB1F, &at) == 0);
    CHECK_EQ(at.group, 1);
    CHECK_EQ(at.min_addr, 0x800);
    CHECK_EQ(at.max_addr, 0xFFF);
    CHECK(mem_bank_pairs(4, addrs, 9, left, right, 8, &pairs) == 0);
    CHECK_EQ(pairs, 1);
    CHECK_EQ(addrs[left[0]], 8004);
    CHECK_EQ(addrs[right[0]], 8000);
    CHECK_EQ(mem_seq_ns(1, 4, 200, 50, 4), 350);
    CHECK_EQ(mem_seq_ns(0, 4, 200, 50, 4), 800);
    CHECK_EQ(mem_seq_ns(1, 2, 200, 50, 4), 450);
    CHECK_EQ(mem_seq_ns(0, 2, 200, 50, 4), 800);
}

static void test_geom_and_walk(void) {
    MemGeom geom;
    MemWalkOut row;
    MemWalkOut col;
    uint32_t block = 0;
    uint32_t tag = 0;
    uint32_t off = 0;
    int set = 0;
    CHECK(mem_cache_geom(28, 8, 1, 64, 0, MEM_REPL_LRU, &geom) == 0);
    CHECK_EQ(geom.tag_bits, 19);
    CHECK_EQ(geom.index_bits, 3);
    CHECK_EQ(geom.offset_bits, 6);
    CHECK_EQ(geom.dirty_bits, 0);
    CHECK_EQ(geom.valid_bits, 1);
    CHECK_EQ(geom.replace_bits, 0);
    CHECK_EQ(geom.total_bits, 4256);
    CHECK(mem_cache_map(8, 1, 64, 444, &block, &set, &tag, &off) == 0);
    CHECK_EQ(set, 6);
    CHECK(mem_cache_map(8, 1, 64, 1348, &block, &set, &tag, &off) == 0);
    CHECK_EQ(set, 5);
    CHECK(mem_cache_geom(32, 1024, 1, 16, 1, MEM_REPL_LRU, &geom) == 0);
    CHECK_EQ(geom.tag_bits, 18);
    CHECK_EQ(geom.dirty_bits, 1);
    CHECK_EQ(geom.total_bits, 151552);
    CHECK(mem_cache_geom(32, 512, 8, 64, 0, MEM_REPL_LRU, &geom) == 0);
    CHECK_EQ(geom.tag_bits, 20);
    CHECK_EQ(geom.replace_bits, 3);
    CHECK_EQ(geom.dirty_bits, 0);
    CHECK(mem_cache_walk(28, 8, 1, 64, 320, 256, 256, &row, &col) == 0);
    CHECK_EQ(row.accesses, 65536);
    CHECK_EQ(row.hits, 61440);
    CHECK_EQ(row.misses, 4096);
    CHECK_EQ(col.hits, 0);
    CHECK_EQ(col.misses, 65536);
}

static void test_writeback(void) {
    MmMachine *m = mm_create(0);
    MemCacheCfg cfg;
    MemAccess info;
    uint32_t word = 0x11223344u;
    uint32_t raw = 0;
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = 2;
    cfg.ways = 1;
    cfg.block_bytes = 4;
    cfg.write_back = 1;
    cfg.write_alloc = 1;
    CHECK(mem_cache_set(m, &cfg) == 0);
    CHECK(mem_cache_access(m, 0, 1, 0, 4, &word, &info) == 0);
    CHECK(mm_raw_read(m, 0, 4, &raw) == 0);
    CHECK_EQ(raw, 0);
    word = 0;
    CHECK(mem_cache_access(m, 0, 0, 8, 4, &word, &info) == 0);
    CHECK_EQ(info.writeback, 1);
    CHECK(mm_raw_read(m, 0, 4, &raw) == 0);
    CHECK_EQ(raw, 0x11223344u);
    mm_destroy(m);
}

static void occupy(MmMachine *m, int replace, int *seen) {
    MemCacheCfg cfg;
    MemAccess info;
    uint32_t word = 0;
    uint32_t seq[] = {0, 4, 0, 8};
    int valid = 0;
    int dirty = 0;
    uint32_t block = 0;
    int i;
    int way;
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = 2;
    cfg.ways = 2;
    cfg.block_bytes = 4;
    cfg.replace = replace;
    cfg.write_alloc = 1;
    CHECK(mem_cache_set(m, &cfg) == 0);
    for (i = 0; i < 4; i++) {
        CHECK(mem_cache_access(m, 0, 0, seq[i], 4, &word, &info) == 0);
    }
    for (way = 0; way < 2; way++) {
        CHECK(mem_cache_peek(m, 0, 0, way, &valid, &dirty, &block) == 0);
        if (valid && block < 4) {
            seen[block] = 1;
        }
    }
}

static void test_replace(void) {
    MmMachine *m = mm_create(0);
    int lru[4] = {0};
    int fifo[4] = {0};
    occupy(m, MEM_REPL_LRU, lru);
    occupy(m, MEM_REPL_FIFO, fifo);
    CHECK_EQ(lru[0], 1);
    CHECK_EQ(lru[1], 0);
    CHECK_EQ(lru[2], 1);
    CHECK_EQ(fifo[0], 0);
    CHECK_EQ(fifo[1], 1);
    CHECK_EQ(fifo[2], 1);
    mm_destroy(m);
}

static int fifth_way(uint32_t seed) {
    MmMachine *m = mm_create(0);
    MemCacheCfg cfg;
    MemAccess info;
    uint32_t word = 0;
    uint32_t addr;
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = 4;
    cfg.ways = 4;
    cfg.block_bytes = 4;
    cfg.replace = MEM_REPL_RAND;
    cfg.seed = seed;
    cfg.write_alloc = 1;
    if (mem_cache_set(m, &cfg) != 0) {
        mm_destroy(m);
        return -2;
    }
    for (addr = 0; addr < 20; addr += 4) {
        if (mem_cache_access(m, 0, 0, addr, 4, &word, &info) != 0) {
            mm_destroy(m);
            return -2;
        }
    }
    mm_destroy(m);
    return info.way;
}

static void test_rand(void) {
    int a = fifth_way(1);
    int b = fifth_way(1);
    int c = fifth_way(2);
    CHECK(a >= 0);
    CHECK_EQ(a, b);
    CHECK(c >= 0);
}

static MmMachine *load_src(const char *src) {
    char err[256];
    AsmImage *img = NULL;
    MmMachine *m;
    err[0] = '\0';
    if (asm_assemble(src, &img, err, sizeof err) != 0) {
        return NULL;
    }
    m = mm_create(0);
    if (m == NULL || mm_load(m, img) != 0) {
        asm_free(img);
        mm_destroy(m);
        return NULL;
    }
    asm_free(img);
    return m;
}

static void test_tick_stall(void) {
    static const char *src = "ld r1, [r2]\nld r1, [r2]\nadd r3, r1, r0\nhlt\n";
    MmMachine *plain = load_src(src);
    MmMachine *cached = load_src(src);
    MemCacheCfg cfg;
    MemCacheStat data;
    MemCacheStat inst;
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = 4;
    cfg.ways = 1;
    cfg.block_bytes = 16;
    cfg.mem_cycles = 5;
    cfg.write_back = 0;
    cfg.write_alloc = 0;
    CHECK(plain != NULL && cached != NULL);
    CHECK(mem_cache_set(cached, &cfg) == 0);
    cpu_run(plain, 20);
    cpu_run(cached, 20);
    mem_cache_stat(cached, 0, &data);
    mem_cache_stat(cached, 1, &inst);
    CHECK_EQ(data.accesses, 2);
    CHECK_EQ(data.hits, 1);
    CHECK_EQ(data.misses, 1);
    CHECK_EQ(inst.accesses, 4);
    CHECK_EQ(inst.hits, 3);
    CHECK_EQ(inst.misses, 1);
    CHECK_EQ(mm_cycles(cached), mm_cycles(plain) + 40);
    mm_destroy(plain);
    mm_destroy(cached);
}

static void fill_ones(MmMachine *m) {
    int i;
    for (i = 0; i < 128; i++) {
        mm_raw_write(m, 8192u + (uint32_t)i * 4u, 4, 1);
    }
}

static void test_one_walk(const char *src, int hits, int misses) {
    MmMachine *m = load_src(src);
    MemCacheCfg cfg;
    MemCacheStat data;
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = 4;
    cfg.ways = 1;
    cfg.block_bytes = 64;
    cfg.mem_cycles = 4;
    cfg.split = 1;
    CHECK(m != NULL);
    fill_ones(m);
    CHECK(mem_cache_set(m, &cfg) == 0);
    cpu_run(m, 100000);
    mem_cache_stat(m, 0, &data);
    CHECK(mm_halted(m));
    CHECK_EQ(mm_reg(m, 0), 128);
    CHECK_EQ(data.accesses, 128);
    CHECK_EQ(data.hits, hits);
    CHECK_EQ(data.misses, misses);
    mm_destroy(m);
}

static void test_array(void) {
    static const char *row = "ld r1, #8192\nld r2, #128\nloop:\nld r3, [r1]\nadd r0, r0, r3\n"
                              "addi r1, r1, #4\nsubi r2, r2, #1\njnz loop\nhlt\n";
    static const char *col = "ld r1, #8192\nld r4, #0\ncol:\nld r5, #0\nrow:\nmov r6, r5\n"
                              "shl r6, r6, #6\nadd r6, r6, r4\nshl r6, r6, #2\nadd r6, r6, r1\n"
                              "ld r3, [r6]\nadd r0, r0, r3\naddi r5, r5, #1\ncmpi r5, #2\njl row\n"
                              "addi r4, r4, #1\ncmpi r4, #64\njl col\nhlt\n";
    test_one_walk(row, 120, 8);
    test_one_walk(col, 0, 128);
}

static void test_pipe_stall(void) {
    static const char *src = "ld r1, [r2]\nld r1, [r2]\nadd r3, r1, r0\nhlt\n";
    MmMachine *plain = load_src(src);
    MmMachine *cached = load_src(src);
    MemCacheCfg cfg;
    CpuPipeOpt opt;
    CpuPipeStat st;
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = 4;
    cfg.block_bytes = 16;
    cfg.mem_cycles = 5;
    cfg.write_back = 0;
    cpu_pipe_opt_default(&opt);
    CHECK(plain != NULL && cached != NULL);
    cpu_pipe_set(plain, &opt, 1);
    cpu_pipe_set(cached, &opt, 1);
    CHECK(mem_cache_set(cached, &cfg) == 0);
    cpu_run(plain, 20);
    cpu_run(cached, 20);
    cpu_pipe_stat(cached, &st);
    CHECK(mm_halted(cached));
    CHECK_EQ(mm_cycles(cached), mm_cycles(plain) + 40);
    CHECK(st.cycles > 0);
    mm_destroy(plain);
    mm_destroy(cached);
}

int main(void) {
    test_chip_and_bank();
    test_geom_and_walk();
    test_writeback();
    test_replace();
    test_rand();
    test_tick_stall();
    test_array();
    test_pipe_stall();
    return TEST_SUMMARY();
}
