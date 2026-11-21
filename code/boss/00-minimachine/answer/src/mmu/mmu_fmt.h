#ifndef MMU_FMT_H
#define MMU_FMT_H

#include <stdint.h>

/* 页 4KB。两级各 10 位时，页目录 1024 项 × 4B，正好一页。 */
#define MMU_PAGE_BYTES 4096u
#define MMU_OFF_BITS 12
#define MMU_IDX_BITS 10

#define MMU_P 1u
#define MMU_W 2u
#define MMU_U 4u
#define MMU_A 8u
#define MMU_D 16u

typedef struct {
    uint32_t dir;
    uint32_t page;
    uint32_t offset;
    uint32_t vpn;
} MmuSplit;

typedef struct {
    int offset_bits;
    int page_bits;
    int level_bits;
    int levels;
    int top_bits;
    int fits;
    unsigned long long pages;
    unsigned long long table_bytes;
    unsigned long long per_page;
    unsigned long long top_entries;
    unsigned long long top_bytes;
    unsigned long long tables_pages;
} MmuGeom;

void mmu_split(uint32_t va, MmuSplit *out);
uint32_t mmu_pack_va(uint32_t dir, uint32_t page, uint32_t offset);
uint32_t mmu_slot(uint32_t base, uint32_t index);
uint32_t mmu_join(uint32_t frame, uint32_t offset);
uint32_t mmu_pack(uint32_t frame, int w, int u, int a, int d);
void mmu_fields(uint32_t raw, int *p, int *w, int *u, int *a, int *d, uint32_t *frame);
int mmu_geom(int va_bits, unsigned long long page, unsigned long long pte, MmuGeom *out);
int mmu_top(uint32_t va, int dir_bits, uint32_t *dir);

#endif
