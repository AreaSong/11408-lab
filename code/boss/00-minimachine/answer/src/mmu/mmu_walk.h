#ifndef MMU_WALK_H
#define MMU_WALK_H

#include "common/mm_machine.h"
#include "mmu/mmu_fmt.h"

#include <stdio.h>

enum { MMU_REPL_LRU = 1, MMU_REPL_FIFO = 2, MMU_REPL_RAND = 3 };

enum { MMU_EAT_AVG = 0, MMU_EAT_TLB = 1, MMU_EAT_PT = 2, MMU_EAT_FAULT = 3 };

enum {
    MMU_STEP_TLB = 1,
    MMU_STEP_PT = 2,
    MMU_STEP_FAULT = 3,
    MMU_STEP_PROT = 4,
    MMU_STEP_MCHK = 5
};

enum { MMU_CACHE_NONE = -1, MMU_CACHE_MISS = 0, MMU_CACHE_HIT = 1, MMU_CACHE_OFF = 2, MMU_CACHE_RANGE = 3 };

typedef struct {
    int entries;
    int ways;
    int replace;
    int walk_cycles;
    uint32_t ptbr;
} MmuCfg;

typedef struct {
    int kind;
    int tlb;
    uint32_t va;
    uint32_t pa;
    uint32_t dir;
    uint32_t page;
    uint32_t offset;
    uint32_t frame;
    uint32_t pde_addr;
    uint32_t pde;
    uint32_t pte_addr;
    uint32_t pte;
    int saw_pde;
    int saw_pte;
    int cache;
} MmuStep;

typedef struct {
    int accesses;
    int tlb_hits;
    int pt_hits;
    int faults;
    int prot;
    int walks;
} MmuStat;

int mmu_enable(MmMachine *m, const MmuCfg *cfg);
void mmu_drop(MmMachine *m);
int mmu_on(const MmMachine *m);
int mmu_map(MmMachine *m, uint32_t va, uint32_t pa, int w, int u);
int mmu_plant(MmMachine *m, int set, int way, int valid, uint32_t tag, uint32_t frame, int w, int u);
void mmu_flush(MmMachine *m);
void mmu_set_fix(MmMachine *m, uint32_t va, uint32_t pa, int w, int u);
int mmu_repair(MmMachine *m);
int mmu_xlate(MmMachine *m, uint32_t va, int writing, MmuStep *out);
int mmu_cpu_read(MmMachine *m, uint32_t va, int size, uint32_t *data);
int mmu_cpu_write(MmMachine *m, uint32_t va, int size, uint32_t data);
int mmu_mem_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mmu_mem_write(MmMachine *m, uint32_t addr, int size, uint32_t data);
int mmu_take_stall(MmMachine *m);
void mmu_stat(const MmMachine *m, MmuStat *out);
void mmu_report(const MmMachine *m, FILE *fp);
int mmu_eat(int kind, unsigned long long hit, unsigned long long den, unsigned long long tlb,
            unsigned long long mem, int walks, unsigned long long fault, unsigned long long *num,
            unsigned long long *den_out);

#endif
