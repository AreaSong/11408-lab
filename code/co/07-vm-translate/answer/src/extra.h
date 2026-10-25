#ifndef EXTRA_H
#define EXTRA_H

#include <stdint.h>
#include <stdio.h>

enum {
    SEG_OK = 0,
    SEG_OVER = 1,
    SEG_FAULT = 2,
    SEG_OFF = 3
};

/* 段内偏移从 0 计。偏移 ≥ 段长就是越界，物理地址 = 段基址 + 段内偏移。 */
int seg_translate(uint64_t base, uint64_t limit, uint64_t offset, int *fault, uint64_t *pa);
int show_seg(FILE *out, uint64_t base, uint64_t limit, uint64_t offset);

/* 段页式：先看页号有没有超出这一段的页表长度，再看页内偏移，最后看存在位。 */
int segpage_translate(int page_bytes, uint64_t n_pages, uint64_t page_no, uint64_t offset, int present,
                      uint64_t frame, int *kind, uint64_t *pa);
int show_segpage(FILE *out, int page_bytes, uint64_t n_pages, uint64_t page_no, uint64_t offset, int present,
                 uint64_t frame);

typedef struct {
    uint64_t vpn;
    uint64_t offset;
    uint64_t pte_va;
    uint64_t pte_pa;
    uint64_t new_frame;
    uint64_t pt_vpn;
    uint64_t pt_pte_va;
    uint64_t pt_frame;
} PteLoc;

int pte_locate(int va_bits, int page_bytes, int pte_bytes, uint64_t pt_va, uint64_t pt_pa, uint64_t va,
               uint64_t final_pa, PteLoc *out);
int show_pte(FILE *out, int va_bits, int page_bytes, int pte_bytes, uint64_t pt_va, uint64_t pt_pa, uint64_t va,
             uint64_t final_pa);

#endif
