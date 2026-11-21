#include "mmu/mmu_tlb.h"

#include <string.h>

static int pow2_bits(int n) {
    int b = 0;
    if (n <= 0 || (n & (n - 1)) != 0) {
        return -1;
    }
    while (n > 1) {
        n >>= 1;
        b++;
    }
    return b;
}

int mmu_tlb_geom(int entries, int ways, int *sets, int *index_bits) {
    int eb = pow2_bits(entries);
    int wb = pow2_bits(ways);
    int sb;
    if (eb < 0 || wb < 0 || ways > entries || entries % ways != 0 || entries > 256) {
        return -1;
    }
    *sets = entries / ways;
    sb = pow2_bits(*sets);
    if (sb < 0) {
        return -1;
    }
    *index_bits = sb;
    return 0;
}

static void cut_vpn(int index_bits, uint32_t vpn, int *set, uint32_t *tag) {
    uint32_t mask = index_bits == 0 ? 0u : (1u << index_bits) - 1u;
    *set = (int)(vpn & mask);
    *tag = vpn >> index_bits;
}

/* 有效位为 0 的项，标记碰巧相同也不算命中。缺页要等页表的 P 位。 */
int mmu_tlb_find(int entries, int ways, uint32_t vpn, const MmuTlbItem *item, int n, uint32_t *frame) {
    int sets = 0;
    int index_bits = 0;
    int set = 0;
    int way;
    uint32_t tag = 0;
    if (item == NULL || n != entries || mmu_tlb_geom(entries, ways, &sets, &index_bits) != 0) {
        return -1;
    }
    cut_vpn(index_bits, vpn, &set, &tag);
    for (way = 0; way < ways; way++) {
        const MmuTlbItem *slot = &item[set * ways + way];
        if (slot->valid && slot->tag == tag) {
            *frame = slot->frame;
            return 1;
        }
    }
    return 0;
}

typedef struct {
    int valid;
    uint32_t tag;
    uint32_t vpn;
    uint32_t stamp;
} SerLine;

static int pick_line(SerLine *base, int ways) {
    int way;
    int old = 0;
    for (way = 0; way < ways; way++) {
        if (!base[way].valid) {
            return way;
        }
    }
    for (way = 1; way < ways; way++) {
        if (base[way].stamp < base[old].stamp) {
            old = way;
        }
    }
    return old;
}

/* 组号 = 虚页号 mod 组数。空行优先，否则换这一组里最久没访问的。 */
int mmu_tlb_series(int entries, int ways, const uint32_t *vpn, int n, int *hits, uint32_t *victim) {
    int sets = 0;
    int index_bits = 0;
    int i;
    int replaced = 0;
    uint32_t clock = 0;
    SerLine line[256];
    if (vpn == NULL || n < 1 || n > 64 || mmu_tlb_geom(entries, ways, &sets, &index_bits) != 0) {
        return -1;
    }
    memset(line, 0, sizeof line);
    *hits = 0;
    for (i = 0; i < n; i++) {
        int set = 0;
        uint32_t tag = 0;
        int way;
        SerLine *base;
        cut_vpn(index_bits, vpn[i], &set, &tag);
        base = &line[set * ways];
        for (way = 0; way < ways; way++) {
            if (base[way].valid && base[way].tag == tag) {
                base[way].stamp = ++clock;
                (*hits)++;
                way = -1;
                break;
            }
        }
        if (way < 0) {
            continue;
        }
        way = pick_line(base, ways);
        if (base[way].valid) {
            if (!replaced) {
                *victim = base[way].vpn;
            }
            replaced = 1;
        }
        base[way].valid = 1;
        base[way].tag = tag;
        base[way].vpn = vpn[i];
        base[way].stamp = ++clock;
    }
    return replaced ? 1 : 0;
}
