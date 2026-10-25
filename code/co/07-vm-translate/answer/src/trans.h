#ifndef TRANS_H
#define TRANS_H

#include "fields.h"
#include "geom.h"

#include <stdint.h>
#include <stdio.h>

#define TRANS_MAX 64

enum {
    VIA_NONE = 0,
    VIA_TLB = 1,
    VIA_PTE = 2,
    VIA_FAULT = 3,
    VIA_NO_PTE = 4
};

/* 下标是 组号 * 路数 + 路号。given 为 0 的路按空项。 */
typedef struct {
    int given;
    int valid;
    uint64_t tag;
    uint64_t frame;
} TlbSlot;

typedef struct {
    uint64_t vpn;
    int present;
    uint64_t frame;
} PteItem;

typedef struct {
    int given;
    int valid;
    uint64_t tag;
} LineSlot;

typedef struct {
    const PageGeom *page;
    const TlbGeom *tlb;
    const TlbSlot *slots;
    const PteItem *ptes;
    int n_pte;
    const Fields *cache;
    const LineSlot *lines;
    uint64_t va;
} TransIn;

typedef struct {
    int via;
    int tlb_set;
    int tlb_way;
    uint64_t vpn;
    uint64_t offset;
    uint64_t tlb_tag;
    uint64_t frame;
    uint64_t pa;
    int cache_on;
    int cache_hit;
    int cache_set;
    int cache_way;
    uint64_t cache_tag;
    uint64_t cache_off;
    int invalid_tag;
} TransOut;

int trans_run(const TransIn *in, TransOut *out);
int show_pt(FILE *out, const PageGeom *g, const PteItem *ptes, int n_pte, uint64_t va);
int show_access(FILE *out, const TransIn *in);

#endif
