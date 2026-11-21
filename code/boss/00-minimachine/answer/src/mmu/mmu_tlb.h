#ifndef MMU_TLB_H
#define MMU_TLB_H

#include <stdint.h>

/* 快表项按组优先、组内按路号排。全相联只有一组，路数等于项数。 */
typedef struct {
    int valid;
    uint32_t tag;
    uint32_t frame;
} MmuTlbItem;

int mmu_tlb_geom(int entries, int ways, int *sets, int *index_bits);
int mmu_tlb_find(int entries, int ways, uint32_t vpn, const MmuTlbItem *item, int n, uint32_t *frame);
int mmu_tlb_series(int entries, int ways, const uint32_t *vpn, int n, int *hits, uint32_t *victim);

#endif
