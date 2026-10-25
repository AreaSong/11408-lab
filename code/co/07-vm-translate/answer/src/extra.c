#include "extra.h"

#include "fmt.h"
#include "geom.h"

#include <stdio.h>

int seg_translate(uint64_t base, uint64_t limit, uint64_t offset, int *fault, uint64_t *pa) {
    if (limit == 0 || offset >= limit) {
        *fault = 1;
        *pa = 0;
        return 0;
    }
    if (base > ~0ull - offset) {
        fprintf(stderr, "段基址加偏移超出 64 位\n");
        return 1;
    }
    *fault = 0;
    *pa = base + offset;
    return 0;
}

int show_seg(FILE *out, uint64_t base, uint64_t limit, uint64_t offset) {
    int fault = 0;
    uint64_t pa = 0;
    char base_text[32];
    char limit_text[32];
    char off_text[32];
    char pa_text[32];
    char pa_h[32];
    if (seg_translate(base, limit, offset, &fault, &pa) != 0) {
        return 1;
    }
    fmt_hex(base_text, sizeof base_text, base, 0);
    fmt_hex(limit_text, sizeof limit_text, limit, 0);
    fmt_hex(off_text, sizeof off_text, offset, 0);
    fmt_hex(pa_text, sizeof pa_text, pa, 0);
    fmt_h(pa_h, sizeof pa_h, pa, 0);
    fprintf(out, "【段式地址】\n");
    fprintf(out, "段基址 %s，段长 %s 字节，段内偏移 %s\n", base_text, limit_text, off_text);
    fprintf(out, "段内偏移从 0 计，合法范围是 0 ≤ 偏移 < 段长\n");
    if (fault) {
        fprintf(out, "偏移 %s 不小于段长 %s\n", off_text, limit_text);
        fprintf(out, "结果：越界\n");
        return 0;
    }
    fprintf(out, "%s < %s，没有越界\n", off_text, limit_text);
    fprintf(out, "物理地址 = 段基址 + 段内偏移 = %s + %s = %s（%s）\n", base_text, off_text, pa_text, pa_h);
    fprintf(out, "结果：物理地址 = %s（%s）\n", pa_text, pa_h);
    return 0;
}

int segpage_translate(int page_bytes, uint64_t n_pages, uint64_t page_no, uint64_t offset, int present,
                      uint64_t frame, int *kind, uint64_t *pa) {
    if (!geom_pow2(page_bytes)) {
        fprintf(stderr, "页大小要是 2 的幂\n");
        return 1;
    }
    if (page_no >= n_pages) {
        *kind = SEG_OVER;
        *pa = 0;
        return 0;
    }
    if (offset >= (uint64_t)page_bytes) {
        *kind = SEG_OFF;
        *pa = 0;
        return 0;
    }
    if (!present) {
        *kind = SEG_FAULT;
        *pa = 0;
        return 0;
    }
    *kind = SEG_OK;
    *pa = (frame << geom_log2(page_bytes)) | offset;
    return 0;
}

static const char *seg_kind_text(int kind) {
    if (kind == SEG_OVER) {
        return "越界";
    }
    if (kind == SEG_FAULT) {
        return "缺页";
    }
    if (kind == SEG_OFF) {
        return "页内偏移超出页";
    }
    return "物理地址";
}

int show_segpage(FILE *out, int page_bytes, uint64_t n_pages, uint64_t page_no, uint64_t offset, int present,
                 uint64_t frame) {
    int kind = 0;
    uint64_t pa = 0;
    int off_bits;
    char off_text[32];
    char frame_text[32];
    char pa_text[32];
    char pa_h[32];
    if (segpage_translate(page_bytes, n_pages, page_no, offset, present, frame, &kind, &pa) != 0) {
        return 1;
    }
    off_bits = geom_pow2(page_bytes) ? geom_log2(page_bytes) : 0;
    fmt_hex(off_text, sizeof off_text, offset, off_bits);
    fmt_hex(frame_text, sizeof frame_text, frame, 0);
    fmt_hex(pa_text, sizeof pa_text, pa, 0);
    fmt_h(pa_h, sizeof pa_h, pa, 0);
    fprintf(out, "【段页式地址】\n");
    fprintf(out, "页 %d B，这一段有 %llu 页，页号 %llu，页内偏移 %s，存在位 %d，页框号 %s\n", page_bytes,
            (unsigned long long)n_pages, (unsigned long long)page_no, off_text, present, frame_text);
    fprintf(out, "段页式先查段表：页号 ≥ 段内页数就是越界，还没轮到存在位\n");
    if (kind == SEG_OVER) {
        fprintf(out, "页号 %llu 不小于 %llu\n", (unsigned long long)page_no, (unsigned long long)n_pages);
        fprintf(out, "结果：越界\n");
        return 0;
    }
    fprintf(out, "页号 %llu < %llu，段内没有越界\n", (unsigned long long)page_no, (unsigned long long)n_pages);
    if (kind == SEG_OFF) {
        fprintf(out, "页内偏移不小于页大小\n");
        fprintf(out, "结果：页内偏移超出页\n");
        return 0;
    }
    if (kind == SEG_FAULT) {
        fprintf(out, "存在位是 0，这一页不在主存\n");
        fprintf(out, "结果：缺页\n");
        return 0;
    }
    fprintf(out, "存在位是 1。物理地址 = 页框号 %s 拼上页内偏移 %s = %s（%s）\n", frame_text, off_text, pa_text, pa_h);
    fprintf(out, "结果：%s = %s（%s）\n", seg_kind_text(kind), pa_text, pa_h);
    return 0;
}

int pte_locate(int va_bits, int page_bytes, int pte_bytes, uint64_t pt_va, uint64_t pt_pa, uint64_t va,
               uint64_t final_pa, PteLoc *out) {
    PageGeom g;
    if (pte_bytes <= 0 || pte_bytes > 256) {
        fprintf(stderr, "页表项大小要在 1 到 256 字节之间\n");
        return 1;
    }
    if (page_geom(&g, va_bits, va_bits, page_bytes) != 0) {
        return 1;
    }
    if (va_split(&g, va, &out->vpn, &out->offset) != 0 || va_split(&g, pt_va, &out->pt_vpn, &out->offset) != 0) {
        return 1;
    }
    if (pt_pa > geom_mask(va_bits) || final_pa > geom_mask(va_bits)) {
        fprintf(stderr, "页表物理地址或结果物理地址超出地址位数\n");
        return 1;
    }
    out->pte_va = pt_va + out->vpn * (uint64_t)pte_bytes;
    out->pte_pa = pt_pa + out->vpn * (uint64_t)pte_bytes;
    out->pt_pte_va = pt_va + out->pt_vpn * (uint64_t)pte_bytes;
    out->new_frame = final_pa >> g.offset_bits;
    out->pt_frame = pt_pa >> g.offset_bits;
    out->offset = va & geom_mask(g.offset_bits);
    return 0;
}

static void show_pte_target(FILE *out, const PageGeom *g, int pte_bytes, uint64_t pt_va, uint64_t pt_pa,
                            const PteLoc *loc) {
    char va_vpn[32];
    char off[32];
    char pte_va[32];
    char pte_pa[32];
    char frame[32];
    char base_va[32];
    char base_pa[32];
    fmt_hex(va_vpn, sizeof va_vpn, loc->vpn, g->vpn_bits);
    fmt_hex(off, sizeof off, loc->offset, g->offset_bits);
    fmt_hex(pte_va, sizeof pte_va, loc->pte_va, g->va_bits);
    fmt_hex(pte_pa, sizeof pte_pa, loc->pte_pa, g->va_bits);
    fmt_hex(frame, sizeof frame, loc->new_frame, g->pfn_bits);
    fmt_hex(base_va, sizeof base_va, pt_va, g->va_bits);
    fmt_hex(base_pa, sizeof base_pa, pt_pa, g->va_bits);
    fprintf(out, "要访问的虚页号 = %s，页内偏移 = %s\n", va_vpn, off);
    fprintf(out, "页表项虚拟地址 = 页表始址 %s + 虚页号 × %d = %s\n", base_va, pte_bytes, pte_va);
    fprintf(out, "页表装在连续的物理地址上，页表项物理地址 = %s + 同一偏移 = %s\n", base_pa, pte_pa);
    fprintf(out, "缺页处理之后的物理地址，页框号 = 高 %d 位 = %s\n", g->pfn_bits, frame);
}

static void show_pte_self(FILE *out, const PageGeom *g, int pte_bytes, uint64_t pt_va, const PteLoc *loc) {
    char pt_vpn[32];
    char self_va[32];
    char frame[32];
    char base[32];
    fmt_hex(pt_vpn, sizeof pt_vpn, loc->pt_vpn, g->vpn_bits);
    fmt_hex(self_va, sizeof self_va, loc->pt_pte_va, g->va_bits);
    fmt_hex(frame, sizeof frame, loc->pt_frame, g->pfn_bits);
    fmt_hex(base, sizeof base, pt_va, g->va_bits);
    fprintf(out, "页表自己所在页的虚页号 = 页表始址的高 %d 位 = %s\n", g->vpn_bits, pt_vpn);
    fprintf(out, "这个页自己的页表项虚拟地址 = %s + %s × %d = %s\n", base, pt_vpn, pte_bytes, self_va);
    fprintf(out, "页表始址的物理页框号 = %s\n", frame);
}

static void show_pte_result(FILE *out, const PageGeom *g, const PteLoc *loc) {
    char pte_va[32];
    char pte_pa[32];
    char frame[32];
    char pt_vpn[32];
    char self_va[32];
    char pt_frame[32];
    char pte_va_h[32];
    char pte_pa_h[32];
    char frame_h[32];
    char pt_vpn_h[32];
    char self_h[32];
    char pt_frame_h[32];
    fmt_hex(pte_va, sizeof pte_va, loc->pte_va, g->va_bits);
    fmt_hex(pte_pa, sizeof pte_pa, loc->pte_pa, g->va_bits);
    fmt_hex(frame, sizeof frame, loc->new_frame, g->pfn_bits);
    fmt_hex(pt_vpn, sizeof pt_vpn, loc->pt_vpn, g->vpn_bits);
    fmt_hex(self_va, sizeof self_va, loc->pt_pte_va, g->va_bits);
    fmt_hex(pt_frame, sizeof pt_frame, loc->pt_frame, g->pfn_bits);
    fmt_h(pte_va_h, sizeof pte_va_h, loc->pte_va, g->va_bits);
    fmt_h(pte_pa_h, sizeof pte_pa_h, loc->pte_pa, g->va_bits);
    fmt_h(frame_h, sizeof frame_h, loc->new_frame, g->pfn_bits);
    fmt_h(pt_vpn_h, sizeof pt_vpn_h, loc->pt_vpn, g->vpn_bits);
    fmt_h(self_h, sizeof self_h, loc->pt_pte_va, g->va_bits);
    fmt_h(pt_frame_h, sizeof pt_frame_h, loc->pt_frame, g->pfn_bits);
    fprintf(out,
            "结果：页表项虚拟地址 = %s（%s），页表项物理地址 = %s（%s），新页框号 = %s（%s），页表所在页号 = %s（%s），"
            "该页页表项虚拟地址 = %s（%s），页框号 = %s（%s）\n",
            pte_va, pte_va_h, pte_pa, pte_pa_h, frame, frame_h, pt_vpn, pt_vpn_h, self_va, self_h, pt_frame,
            pt_frame_h);
}

int show_pte(FILE *out, int va_bits, int page_bytes, int pte_bytes, uint64_t pt_va, uint64_t pt_pa, uint64_t va,
             uint64_t final_pa) {
    PageGeom g;
    PteLoc loc;
    uint64_t table_bytes;
    if (page_geom(&g, va_bits, va_bits, page_bytes) != 0 ||
        pte_locate(va_bits, page_bytes, pte_bytes, pt_va, pt_pa, va, final_pa, &loc) != 0) {
        return 1;
    }
    fprintf(out, "【页表项在哪】\n");
    if (page_bytes % (1 << 20) == 0) {
        fprintf(out, "按字节编址，地址 %d 位，页 %d B = %d MB（M = 2^20），页表项 %d B\n", va_bits, page_bytes,
                page_bytes / (1 << 20), pte_bytes);
    } else {
        fprintf(out, "按字节编址，地址 %d 位，页 %d B，页表项 %d B\n", va_bits, page_bytes, pte_bytes);
    }
    fprintf(out, "页内偏移 = log2(%d) = %d 位，虚页号 = %d - %d = %d 位\n", page_bytes, g.offset_bits, va_bits,
            g.offset_bits, g.vpn_bits);
    if (g.vpn_bits < 32) {
        table_bytes = (1ull << g.vpn_bits) * (uint64_t)pte_bytes;
        fprintf(out, "页表项有 2^%d 个，整张页表 %llu B\n", g.vpn_bits, (unsigned long long)table_bytes);
        if (table_bytes <= (uint64_t)page_bytes) {
            fprintf(out, "整张页表不超过一页，才能按「始址 + 页号 × 页表项大小」连续往下算\n");
        }
    }
    show_pte_target(out, &g, pte_bytes, pt_va, pt_pa, &loc);
    show_pte_self(out, &g, pte_bytes, pt_va, &loc);
    fprintf(out, "页表项地址用的是页号乘页表项字节数，不是乘页大小\n");
    show_pte_result(out, &g, &loc);
    return 0;
}
