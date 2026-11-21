#include "asm/asm_asm.h"
#include "common/mm_machine.h"
#include "cpu/cpu_exec.h"
#include "kernel/k_trap.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_fmt.h"
#include "mmu/mmu_tlb.h"
#include "mmu/mmu_walk.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_tables(void) {
    MmuGeom g;
    MmuSplit sp;
    uint32_t da = 0;
    uint32_t db = 0;
    CHECK(mmu_geom(32, 4096, 4, &g) == 0);
    CHECK_EQ(g.pages, 1048576);
    CHECK_EQ(g.table_bytes, 4194304);
    CHECK_EQ(g.levels, 2);
    CHECK_EQ(g.top_entries, 1024);
    CHECK_EQ(g.top_bytes, 4096);
    CHECK_EQ(g.fits, 1);
    CHECK_EQ(g.tables_pages, 1025);
    CHECK(mmu_geom(26, 1024, 2, &g) == 0);
    CHECK_EQ(g.top_entries, 128);
    CHECK_EQ(g.tables_pages, 129);
    CHECK_EQ(g.fits, 1);
    mmu_split(0x03FFF180u, &sp);
    CHECK_EQ(sp.vpn, 0x03FFFu);
    CHECK_EQ(sp.offset, 0x180u);
    CHECK_EQ(mmu_pack_va(6, 6, 8), 0x01806008u);
    CHECK_EQ(mmu_slot(0x00200000u, 8), 0x00200020u);
    CHECK_EQ(mmu_slot(0x00200000u, 9), 0x00200024u);
    CHECK_EQ(mmu_join(0x900u, 0), 0x00900000u);
    CHECK_EQ(mmu_join(0x901u, 0), 0x00901000u);
    CHECK(mmu_top(0x01000000u, 10, &da) == 0);
    CHECK(mmu_top(0x01112048u, 10, &db) == 0);
    CHECK_EQ(da, 4);
    CHECK_EQ(db, 4);
    mmu_split(0x10801008u, &sp);
    CHECK_EQ(sp.dir, 0x42u);
    CHECK_EQ(sp.page, 1u);
    CHECK_EQ(mmu_slot(0x00201000u, sp.dir), 0x00201108u);
    CHECK_EQ(mmu_slot(mmu_join(0x301u, 0), sp.page), 0x00301004u);
}

static void test_tlb_exam(void) {
    MmuTlbItem item[4] = {{0, 0xFF180u, 0x2u}, {1, 0x3FFF1u, 0x35u}, {0, 0x02FF3u, 0x351u}, {1, 0x03FFFu, 0x153u}};
    uint32_t vpn[] = {10, 12, 16, 7, 26, 4, 12, 20};
    uint32_t frame = 0;
    uint32_t victim = 0;
    int hits = 0;
    unsigned long long num = 0;
    unsigned long long den = 0;
    CHECK_EQ(mmu_tlb_find(4, 4, 0x03FFFu, item, 4, &frame), 1);
    CHECK_EQ(frame, 0x153u);
    CHECK_EQ(mmu_join(frame, 0x180u), 0x0153180u);
    CHECK_EQ(mmu_tlb_series(16, 2, vpn, 8, &hits, &victim), 1);
    CHECK_EQ(victim, 4u);
    CHECK_EQ(hits, 1);
    CHECK(mmu_eat(MMU_EAT_TLB, 0, 0, 10, 100, 1, 0, &num, &den) == 0);
    CHECK_EQ(num, 110);
    CHECK(mmu_eat(MMU_EAT_PT, 0, 0, 10, 100, 1, 0, &num, &den) == 0);
    CHECK_EQ(num, 210);
    CHECK(mmu_eat(MMU_EAT_FAULT, 0, 0, 10, 100, 1, 100000000ULL, &num, &den) == 0);
    CHECK_EQ(num, 100000220ULL);
    CHECK(mmu_eat(MMU_EAT_PT, 0, 0, 10, 100, 2, 0, &num, &den) == 0);
    CHECK_EQ(num, 310);
    CHECK(mmu_eat(MMU_EAT_AVG, 9, 10, 20, 100, 2, 0, &num, &den) == 0);
    CHECK_EQ(num, 140);
    CHECK_EQ(den, 1);
}

static void enable(MmMachine *m, int cycles) {
    MmuCfg cfg;
    cfg.entries = 16;
    cfg.ways = 16;
    cfg.replace = MMU_REPL_LRU;
    cfg.walk_cycles = cycles;
    cfg.ptbr = 0x00080000u;
    CHECK(mmu_enable(m, &cfg) == 0);
    mm_set_psw(m, MM_PSW_PG);
}

static void test_walk(void) {
    MmMachine *m = mm_create(0);
    MmuStep step;
    MmuStat st;
    uint32_t word = 0;
    uint32_t pte = 0;
    enable(m, 5);
    CHECK(mmu_map(m, 0x2000u, 0x3000u, 1, 1) == 0);
    CHECK(mm_raw_write(m, 0x3000u, 4, 0x11111111u) == 0);
    CHECK_EQ(mmu_cpu_read(m, 0x2000u, 4, &word), 0);
    CHECK_EQ(word, 0x11111111u);
    CHECK_EQ(mmu_take_stall(m), 10);
    CHECK_EQ(mmu_cpu_read(m, 0x2000u, 4, &word), 0);
    CHECK_EQ(mmu_take_stall(m), 0);
    CHECK(mm_raw_read(m, 0x00081008u, 4, &pte) == 0);
    CHECK_EQ(pte & MMU_A, MMU_A);
    CHECK_EQ(pte & MMU_D, 0);
    CHECK_EQ(mmu_cpu_write(m, 0x2000u, 4, 0x22222222u), 0);
    CHECK(mm_raw_read(m, 0x3000u, 4, &word) == 0);
    CHECK_EQ(word, 0x22222222u);
    CHECK(mm_raw_read(m, 0x00081008u, 4, &pte) == 0);
    CHECK_EQ(pte & MMU_D, MMU_D);
    mmu_flush(m);
    CHECK_EQ(mmu_xlate(m, 0x2000u, 0, &step), 0);
    CHECK_EQ(step.kind, MMU_STEP_PT);
    CHECK_EQ(mmu_cpu_read(m, 0x6000u, 4, &word), MM_EX_PAGE);
    CHECK_EQ(mm_badva(m), 0x6000u);
    mmu_stat(m, &st);
    CHECK(st.tlb_hits >= 1);
    CHECK(st.faults >= 1);
    mm_destroy(m);
}

static void test_prot_and_cache(void) {
    MmMachine *m = mm_create(0);
    MemCacheCfg cfg;
    MemCacheStat st;
    uint32_t word = 0;
    enable(m, 1);
    CHECK(mmu_map(m, 0x4000u, 0x5000u, 1, 0) == 0);
    mm_set_psw(m, MM_PSW_PG | MM_PSW_U);
    CHECK_EQ(mmu_cpu_read(m, 0x4000u, 4, &word), MM_EX_PROT);
    mm_set_psw(m, MM_PSW_PG);
    CHECK_EQ(mmu_cpu_read(m, 0x4000u, 4, &word), 0);
    CHECK(mmu_map(m, 0x2000u, 0x3000u, 0, 1) == 0);
    CHECK_EQ(mmu_cpu_write(m, 0x2000u, 4, 1), MM_EX_PROT);
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    CHECK(mem_cache_set(m, &cfg) == 0);
    CHECK(mm_raw_write(m, 0x5000u, 4, 7) == 0);
    CHECK_EQ(mmu_cpu_read(m, 0x4000u, 4, &word), 0);
    CHECK_EQ(mmu_cpu_read(m, 0x4000u, 4, &word), 0);
    mem_cache_stat(m, 0, &st);
    CHECK_EQ(st.hits, 1);
    CHECK_EQ(st.misses, 1);
    mm_destroy(m);
}

static int load_src(MmMachine *m, const char *src) {
    char err[128];
    AsmImage *img = NULL;
    int rc = asm_assemble(src, &img, err, sizeof err);
    if (rc == 0) {
        rc = mm_load(m, img);
    }
    asm_free(img);
    return rc;
}

static void test_restart(void) {
    MmMachine *m = mm_create(0);
    CHECK(load_src(m, "ld r1, [r2]\nhlt\n") == 0);
    CHECK(k_boot(m) == 0);
    enable(m, 1);
    CHECK(mmu_map(m, 0x1000u, 0x1000u, 1, 1) == 0);
    mmu_set_fix(m, 0x2000u, 0x3000u, 1, 1);
    CHECK(mm_raw_write(m, 0x3000u, 4, 0x2Au) == 0);
    mm_set_reg(m, 2, 0x2000u);
    mm_set_psw(m, MM_PSW_PG);
    CHECK_EQ(cpu_run(m, 20), 0);
    CHECK_EQ(mm_reg(m, 1), 0x2Au);
    CHECK(mm_halted(m));
    CHECK_EQ(mm_cause(m), -1);
    mm_destroy(m);
}

static void test_trace(void) {
    MmMachine *m = mm_create(0);
    FILE *fp = tmpfile();
    char line[128];
    uint32_t word = 0;
    enable(m, 1);
    CHECK(mmu_map(m, 0x2000u, 0x2000u, 1, 1) == 0);
    mm_set_trace(m, fp);
    CHECK_EQ(mmu_cpu_read(m, 0x2000u, 4, &word), 0);
    rewind(fp);
    CHECK(fgets(line, sizeof line, fp) != NULL);
    CHECK(strstr(line, "翻译") != NULL);
    CHECK(strstr(line, "PA=00002000") != NULL);
    fclose(fp);
    mm_destroy(m);
}

int main(void) {
    test_tables();
    test_tlb_exam();
    test_walk();
    test_prot_and_cache();
    test_restart();
    test_trace();
    return TEST_SUMMARY();
}
