#ifndef GEOM_H
#define GEOM_H

#include <stdint.h>
#include <stdio.h>

/* 页式地址。pa_bits 为 0 表示题目没给物理地址位数，页框号位数未知。 */
typedef struct {
    int va_bits;
    int pa_bits;
    int page_bytes;
    int offset_bits;
    int vpn_bits;
    int pfn_bits;
} PageGeom;

/* TLB 和 Cache 同一套划分：标记 | 组号，只是 TLB 比的是虚页号，没有块内偏移。 */
typedef struct {
    int vpn_bits;
    int n_lines;
    int n_ways;
    int n_sets;
    int index_bits;
    int tag_bits;
} TlbGeom;

int geom_pow2(int x);
int geom_log2(int x);
uint64_t geom_mask(int bits);

int page_geom(PageGeom *out, int va_bits, int pa_bits, int page_bytes);
int va_split(const PageGeom *g, uint64_t va, uint64_t *vpn, uint64_t *offset);
int pa_join(const PageGeom *g, uint64_t frame, uint64_t offset, uint64_t *pa);

int tlb_geom(TlbGeom *out, int vpn_bits, int n_lines, int n_ways);
int tlb_parts(const TlbGeom *g, uint64_t vpn, int *set, uint64_t *tag);

int show_page(FILE *out, const PageGeom *g);
int show_cut(FILE *out, const PageGeom *g, uint64_t va);
int show_tlb(FILE *out, const PageGeom *g, const TlbGeom *t);

#endif
