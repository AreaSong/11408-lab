#ifndef MMU_I_H
#define MMU_I_H

#include <stdint.h>

typedef struct {
    int valid;
    uint32_t tag;
    uint32_t vpn;
    uint32_t frame;
    uint32_t pte_addr;
    int w;
    int u;
    int a;
    int d;
    uint32_t stamp;
} MmuLine;

typedef struct Mmu {
    MmuLine *line;
    int entries;
    int ways;
    int sets;
    int index_bits;
    int replace;
    int walk_cycles;
    uint32_t ptbr;
    uint32_t next_pt;
    uint32_t clock;
    uint32_t seed;
    int accesses;
    int tlb_hits;
    int pt_hits;
    int faults;
    int prot;
    int walks;
    int fix_on;
    int fix_w;
    int fix_u;
    uint32_t fix_va;
    uint32_t fix_pa;
} Mmu;

#endif
