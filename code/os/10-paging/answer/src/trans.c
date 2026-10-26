#include "trans.h"

#include "addr.h"

static int bit01(int value) {
    return value == 0 || value == 1;
}

int trans_flat(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long length,
               unsigned long long base, unsigned long long page, int valid, unsigned long long frame,
               unsigned long long offset, FlatInfo *info) {
    if (!bit01(valid) || offset >= page_bytes) {
        return -1;
    }
    /* 页表长度在寄存器里。页号从 0 起，等于长度已经越界，不必再读页表。 */
    if (page >= length) {
        info->result = FLAT_BOUND;
        return 0;
    }
    if (addr_slot(pte_bytes, base, page, &info->pte) != 0) {
        return -1;
    }
    if (!valid) {
        info->result = FLAT_FAULT;
        return 0;
    }
    if (addr_join(page_bytes, frame, offset, &info->pa) != 0) {
        return -1;
    }
    info->result = FLAT_OK;
    return 0;
}

int trans_two(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long dir_base,
              unsigned long long dir, int dir_valid, unsigned long long pt_frame, unsigned long long page,
              int page_valid, unsigned long long frame, unsigned long long offset, TwoInfo *info) {
    if (!bit01(dir_valid) || !bit01(page_valid) || offset >= page_bytes) {
        return -1;
    }
    if (addr_slot(pte_bytes, dir_base, dir, &info->pde) != 0) {
        return -1;
    }
    if (!dir_valid) {
        info->result = TWO_FAULT_DIR;
        return 0;
    }
    /* 页目录里的页框号拼上 0 偏移，才是这一张二级页表的物理始址。 */
    if (addr_join(page_bytes, pt_frame, 0, &info->pt_base) != 0) {
        return -1;
    }
    if (addr_slot(pte_bytes, info->pt_base, page, &info->pte) != 0) {
        return -1;
    }
    if (!page_valid) {
        info->result = TWO_FAULT_PAGE;
        return 0;
    }
    if (addr_join(page_bytes, frame, offset, &info->pa) != 0) {
        return -1;
    }
    info->result = TWO_OK;
    return 0;
}

int trans_look(unsigned long long page_bytes, unsigned long long page, unsigned long long offset,
               int tlb_valid, unsigned long long tlb_page, unsigned long long tlb_frame, int pt_valid,
               unsigned long long pt_frame, LookInfo *info) {
    unsigned long long frame = 0;

    if (!bit01(tlb_valid) || !bit01(pt_valid) || offset >= page_bytes) {
        return -1;
    }
    /* 有效位为 0 的项即使页号相同也是空项，不能据此判命中或判缺页。 */
    if (tlb_valid && tlb_page == page) {
        frame = tlb_frame;
        info->result = LOOK_TLB;
    } else if (pt_valid) {
        frame = pt_frame;
        info->result = LOOK_PT;
    } else {
        info->result = LOOK_FAULT;
        return 0;
    }
    info->frame = frame;
    return addr_join(page_bytes, frame, offset, &info->pa);
}
