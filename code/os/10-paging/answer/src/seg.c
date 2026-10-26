#include "seg.h"

#include "addr.h"
#include "io.h"

int seg_translate(unsigned long long base, unsigned long long limit, unsigned long long offset,
                  int *result, unsigned long long *pa) {
    /* 段内偏移从 0 编到段长减 1。等于段长已经越出这一段。 */
    if (offset >= limit) {
        *result = SEG_BOUND;
        return 0;
    }
    if (add_ull(base, offset, pa) != 0) {
        return -1;
    }
    *result = SEG_OK;
    return 0;
}

int seg_len(int addr_bits, int seg_bits, unsigned long long *segments, unsigned long long *max_len) {
    int offset;

    if (addr_bits < 2 || addr_bits > 63 || seg_bits < 1 || seg_bits >= addr_bits) {
        return -1;
    }
    offset = addr_bits - seg_bits;
    *segments = 1ULL << seg_bits;
    *max_len = 1ULL << offset;
    return 0;
}

int seg_check(unsigned long long seg_count, unsigned long long seg, unsigned long long limit,
              unsigned long long offset, unsigned long long base, int *result, unsigned long long *pa) {
    if (seg_count == 0) {
        return -1;
    }
    if (seg >= seg_count) {
        *result = SEG_BOUND_NO;
        return 0;
    }
    if (offset >= limit) {
        *result = SEG_BOUND_OFF;
        return 0;
    }
    if (add_ull(base, offset, pa) != 0) {
        return -1;
    }
    *result = SEG_OK;
    return 0;
}

int seg_page(unsigned long long page_bytes, unsigned long long pages, unsigned long long page,
             unsigned long long offset, int valid, unsigned long long frame, int tlb_hit,
             SegPageInfo *info) {
    if ((valid != 0 && valid != 1) || (tlb_hit != 0 && tlb_hit != 1) || pages == 0) {
        return -1;
    }
    if (offset >= page_bytes) {
        return -1;
    }
    /* 快表里只会留下合法且在内存中的页。页号越界却声称命中，输入自相矛盾。 */
    if (tlb_hit && (page >= pages || !valid)) {
        return -2;
    }
    if (page >= pages) {
        info->result = SP_BOUND;
        info->visits = 1;
        return 0;
    }
    if (tlb_hit) {
        info->result = SP_TLB;
        info->visits = 1;
        return addr_join(page_bytes, frame, offset, &info->pa);
    }
    if (!valid) {
        info->result = SP_FAULT;
        info->visits = 2;
        return 0;
    }
    info->result = SP_PT;
    info->visits = 3;
    return addr_join(page_bytes, frame, offset, &info->pa);
}
