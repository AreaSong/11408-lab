#include "geom.h"

#include "io.h"

#include <string.h>

/* 低位各级取满「一页能放下的项数」那么多位，剩下的高位给顶级。
 * 顶级项数因此不超过一页，这是多级页表最少要分几级的理由。 */
static int choose_levels(int page_bits, int level_bits, Geom *out) {
    out->levels = (page_bits + level_bits - 1) / level_bits;
    out->top_bits = page_bits % level_bits;
    if (out->top_bits == 0) {
        out->top_bits = level_bits;
    }
    return out->levels <= GEOM_LEVELS ? 0 : -1;
}

static int fill_terms(Geom *out) {
    unsigned long long acc = 1;
    unsigned long long sum = 0;
    int i;

    for (i = 0; i < out->levels; i++) {
        int width = (i == 0) ? out->top_bits : out->level_bits;
        out->terms[i] = acc;
        if (add_ull(sum, acc, &sum) != 0) {
            out->nterms = i + 1;
            out->terms_overflow = 1;
            return 0;
        }
        if (width >= 64 || acc > (unsigned long long)(~0ULL >> width)) {
            out->nterms = i + 1;
            out->terms_overflow = 1;
            return 0;
        }
        acc <<= width;
    }
    out->nterms = out->levels;
    out->tables_pages = sum;
    return 0;
}

static int fill_geom(Geom *out, int page_bits, int offset_bits, unsigned long long page,
                     unsigned long long pte) {
    int level_bits = 0;
    unsigned long long per;

    memset(out, 0, sizeof *out);
    if (pow2_bits(page, &out->offset_bits) != 0 || out->offset_bits != offset_bits) {
        return -1;
    }
    if (pte == 0 || page % pte != 0) {
        return -1;
    }
    per = page / pte;
    /* 一页至少放两项，否则页号无法再切成下一级。 */
    if (pow2_bits(per, &level_bits) != 0 || level_bits < 1) {
        return -1;
    }
    if (page_bits < 0 || page_bits > 63) {
        return -1;
    }
    out->page_bits = page_bits;
    out->level_bits = level_bits;
    out->per_page = per;
    out->page_bytes = page;
    out->pte_bytes = pte;
    out->va_bits = page_bits + offset_bits;
    out->pages = 1ULL << page_bits;
    out->fits = 1;
    if (page_bits == 0) {
        return 0;
    }
    if (mul_ull(out->pages, pte, &out->table_bytes) != 0) {
        out->table_overflow = 1;
    }
    if (choose_levels(page_bits, level_bits, out) != 0) {
        return -1;
    }
    out->top_entries = 1ULL << out->top_bits;
    if (mul_ull(out->top_entries, pte, &out->top_bytes) != 0) {
        return -1;
    }
    out->fits = out->top_bytes <= page;
    return fill_terms(out);
}

int geom_bits(int va_bits, unsigned long long page, unsigned long long pte, Geom *out) {
    int offset = 0;

    if (va_bits < 1 || va_bits > 64 || pow2_bits(page, &offset) != 0) {
        return -1;
    }
    if (offset > va_bits || va_bits - offset > 63) {
        return -1;
    }
    return fill_geom(out, va_bits - offset, offset, page, pte);
}

int geom_pages(unsigned long long logical_pages, unsigned long long page, unsigned long long pte,
               Geom *out) {
    int page_bits = 0;
    int offset = 0;

    if (pow2_bits(logical_pages, &page_bits) != 0 || pow2_bits(page, &offset) != 0) {
        return -1;
    }
    if (page_bits > 63 || page_bits + offset > 64) {
        return -1;
    }
    return fill_geom(out, page_bits, offset, page, pte);
}
