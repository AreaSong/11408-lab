#include "mmu/mmu_fmt.h"

#include <string.h>

static int pow2_bits(unsigned long long n, int *bits) {
    int b = 0;
    if (n == 0 || (n & (n - 1)) != 0) {
        return -1;
    }
    while (n > 1) {
        n >>= 1;
        b++;
    }
    *bits = b;
    return 0;
}

void mmu_split(uint32_t va, MmuSplit *out) {
    out->offset = va & 0xfffu;
    out->page = (va >> MMU_OFF_BITS) & 0x3ffu;
    out->dir = va >> (MMU_OFF_BITS + MMU_IDX_BITS);
    out->vpn = va >> MMU_OFF_BITS;
}

uint32_t mmu_pack_va(uint32_t dir, uint32_t page, uint32_t offset) {
    return (dir << 22) | ((page & 0x3ffu) << 12) | (offset & 0xfffu);
}

/* 表项地址 = 表始址 + 下标 × 4。页目录、页表都是这一条。 */
uint32_t mmu_slot(uint32_t base, uint32_t index) { return base + index * 4u; }

uint32_t mmu_join(uint32_t frame, uint32_t offset) {
    return (frame << MMU_OFF_BITS) | (offset & 0xfffu);
}

uint32_t mmu_pack(uint32_t frame, int w, int u, int a, int d) {
    uint32_t raw = (frame << MMU_OFF_BITS) | MMU_P;
    if (w) {
        raw |= MMU_W;
    }
    if (u) {
        raw |= MMU_U;
    }
    if (a) {
        raw |= MMU_A;
    }
    if (d) {
        raw |= MMU_D;
    }
    return raw;
}

void mmu_fields(uint32_t raw, int *p, int *w, int *u, int *a, int *d, uint32_t *frame) {
    *p = (raw & MMU_P) != 0;
    *w = (raw & MMU_W) != 0;
    *u = (raw & MMU_U) != 0;
    *a = (raw & MMU_A) != 0;
    *d = (raw & MMU_D) != 0;
    *frame = raw >> MMU_OFF_BITS;
}

int mmu_top(uint32_t va, int dir_bits, uint32_t *dir) {
    if (dir_bits < 1 || dir_bits > 31) {
        return -1;
    }
    *dir = va >> (32 - dir_bits);
    return 0;
}

static int table_sum(MmuGeom *out) {
    unsigned long long acc = 1;
    unsigned long long sum = 0;
    int i;
    for (i = 0; i < out->levels; i++) {
        int width = (i == 0) ? out->top_bits : out->level_bits;
        if (sum > ~0ULL - acc) {
            return -1;
        }
        sum += acc;
        if (width >= 63 || acc > (~0ULL >> width)) {
            return -1;
        }
        acc <<= width;
    }
    out->tables_pages = sum;
    return 0;
}

/* 低位各级取满「一页能放的项数」，剩下的高位给顶级，顶级因此不超过一页。 */
int mmu_geom(int va_bits, unsigned long long page, unsigned long long pte, MmuGeom *out) {
    int offset_bits = 0;
    int level_bits = 0;
    unsigned long long per;
    memset(out, 0, sizeof *out);
    if (va_bits < 2 || va_bits > 63 || pow2_bits(page, &offset_bits) != 0) {
        return -1;
    }
    if (offset_bits < 1 || offset_bits >= va_bits || pte == 0 || page % pte != 0) {
        return -1;
    }
    per = page / pte;
    if (pow2_bits(per, &level_bits) != 0 || level_bits < 1) {
        return -1;
    }
    out->offset_bits = offset_bits;
    out->page_bits = va_bits - offset_bits;
    out->level_bits = level_bits;
    out->per_page = per;
    out->pages = 1ULL << out->page_bits;
    if (out->pages > ~0ULL / pte) {
        return -1;
    }
    out->table_bytes = out->pages * pte;
    out->levels = (out->page_bits + level_bits - 1) / level_bits;
    out->top_bits = out->page_bits % level_bits;
    if (out->top_bits == 0) {
        out->top_bits = level_bits;
    }
    out->top_entries = 1ULL << out->top_bits;
    if (out->top_entries > ~0ULL / pte) {
        return -1;
    }
    out->top_bytes = out->top_entries * pte;
    out->fits = out->top_bytes <= page;
    return table_sum(out);
}
