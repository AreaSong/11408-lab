#include "geom.h"

#include "fmt.h"

#include <stdio.h>
#include <string.h>

int geom_pow2(int x) {
    return x > 0 && (x & (x - 1)) == 0;
}

int geom_log2(int x) {
    int n = 0;
    while (x > 1) {
        x >>= 1;
        n++;
    }
    return n;
}

uint64_t geom_mask(int bits) {
    if (bits <= 0) {
        return 0;
    }
    if (bits >= 64) {
        return ~0ull;
    }
    return (1ull << bits) - 1ull;
}

static int bits_ok(int bits) {
    return bits > 0 && bits <= 62;
}

int page_geom(PageGeom *out, int va_bits, int pa_bits, int page_bytes) {
    if (out == NULL) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    if (!bits_ok(va_bits) || !geom_pow2(page_bytes) || page_bytes > (1 << 30)) {
        fprintf(stderr, "虚拟地址位数要在 1 到 62 之间，页大小要是不超过 2^30 的 2 的幂\n");
        return 1;
    }
    out->va_bits = va_bits;
    out->page_bytes = page_bytes;
    out->offset_bits = geom_log2(page_bytes);
    if (out->offset_bits >= va_bits) {
        fprintf(stderr, "页内偏移不能占满整个虚拟地址\n");
        return 1;
    }
    out->vpn_bits = va_bits - out->offset_bits;
    if (pa_bits == 0) {
        out->pa_bits = 0;
        out->pfn_bits = -1;
        return 0;
    }
    if (!bits_ok(pa_bits) || out->offset_bits >= pa_bits) {
        fprintf(stderr, "物理地址位数要能放下页内偏移\n");
        return 1;
    }
    out->pa_bits = pa_bits;
    out->pfn_bits = pa_bits - out->offset_bits;
    return 0;
}

int va_split(const PageGeom *g, uint64_t va, uint64_t *vpn, uint64_t *offset) {
    if (va > geom_mask(g->va_bits)) {
        fprintf(stderr, "虚拟地址超出 %d 位\n", g->va_bits);
        return 1;
    }
    *offset = va & geom_mask(g->offset_bits);
    *vpn = g->offset_bits == 0 ? va : (va >> g->offset_bits);
    return 0;
}

int pa_join(const PageGeom *g, uint64_t frame, uint64_t offset, uint64_t *pa) {
    if (g->pfn_bits < 0) {
        fprintf(stderr, "拼接物理地址需要物理地址位数\n");
        return 1;
    }
    if (frame > geom_mask(g->pfn_bits) || offset > geom_mask(g->offset_bits)) {
        fprintf(stderr, "页框号或页内偏移超出各自的字段\n");
        return 1;
    }
    *pa = (frame << g->offset_bits) | offset;
    return 0;
}

int tlb_geom(TlbGeom *out, int vpn_bits, int n_lines, int n_ways) {
    if (out == NULL) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    if (vpn_bits <= 0 || vpn_bits > 62 || !geom_pow2(n_lines) || !geom_pow2(n_ways) || n_ways > n_lines ||
        n_lines > 4096) {
        fprintf(stderr, "TLB 项数和路数都要是 2 的幂，路数不能大于项数，项数不超过 4096\n");
        return 1;
    }
    out->vpn_bits = vpn_bits;
    out->n_lines = n_lines;
    out->n_ways = n_ways;
    out->n_sets = n_lines / n_ways;
    out->index_bits = geom_log2(out->n_sets);
    if (out->index_bits > vpn_bits) {
        fprintf(stderr, "虚页号的位数装不下 TLB 组号\n");
        return 1;
    }
    out->tag_bits = vpn_bits - out->index_bits;
    return 0;
}

int tlb_parts(const TlbGeom *g, uint64_t vpn, int *set, uint64_t *tag) {
    if (vpn > geom_mask(g->vpn_bits)) {
        fprintf(stderr, "虚页号超出 %d 位\n", g->vpn_bits);
        return 1;
    }
    *set = (int)(vpn & geom_mask(g->index_bits));
    *tag = g->index_bits == 0 ? vpn : (vpn >> g->index_bits);
    return 0;
}

static void show_widths(FILE *out, const PageGeom *g) {
    fprintf(out, "按字节编址，虚拟地址 %d 位，物理地址 %d 位，页 %d B\n", g->va_bits, g->pa_bits, g->page_bytes);
    fprintf(out, "页内偏移 = log2(%d) = %d 位。虚地址和物理地址的这一段相同，翻译时原样留下\n", g->page_bytes,
            g->offset_bits);
    fprintf(out, "虚页号 = %d - %d = %d 位\n", g->va_bits, g->offset_bits, g->vpn_bits);
    fprintf(out, "页框号 = %d - %d = %d 位。它由物理地址位数决定，和虚拟地址有多少位无关\n", g->pa_bits,
            g->offset_bits, g->pfn_bits);
}

int show_page(FILE *out, const PageGeom *g) {
    fprintf(out, "【页式地址】\n");
    show_widths(out, g);
    fprintf(out, "结果：虚页号 = %d 位，页内偏移 = %d 位，页框号 = %d 位\n", g->vpn_bits, g->offset_bits,
            g->pfn_bits);
    return 0;
}

int show_cut(FILE *out, const PageGeom *g, uint64_t va) {
    uint64_t vpn = 0;
    uint64_t offset = 0;
    char va_text[32];
    char vpn_text[32];
    char off_text[32];
    if (va_split(g, va, &vpn, &offset) != 0) {
        return 1;
    }
    fmt_hex(va_text, sizeof va_text, va, g->va_bits);
    fmt_hex(vpn_text, sizeof vpn_text, vpn, g->vpn_bits);
    fmt_hex(off_text, sizeof off_text, offset, g->offset_bits);
    fprintf(out, "【拆分虚拟地址】\n");
    show_widths(out, g);
    fprintf(out, "虚拟地址 %s\n", va_text);
    fprintf(out, "虚页号 = 高 %d 位 = %s = %llu\n", g->vpn_bits, vpn_text, (unsigned long long)vpn);
    fprintf(out, "页内偏移 = 低 %d 位 = %s\n", g->offset_bits, off_text);
    fprintf(out, "十六进制里，低 %d 位正好是末尾 %d 个十六进制数字\n", g->offset_bits, (g->offset_bits + 3) / 4);
    fprintf(out, "结果：虚页号 = %s = %llu，页内偏移 = %s\n", vpn_text, (unsigned long long)vpn, off_text);
    return 0;
}

int show_tlb(FILE *out, const PageGeom *g, const TlbGeom *t) {
    fprintf(out, "【TLB 字段】\n");
    show_widths(out, g);
    fprintf(out, "TLB %d 项，每组 %d 路，%s\n", t->n_lines, t->n_ways,
            t->n_sets <= 1 ? "全相联" : (t->n_ways <= 1 ? "直接映射" : "组相联"));
    fprintf(out, "组数 = %d / %d = %d\n", t->n_lines, t->n_ways, t->n_sets);
    if (t->n_sets <= 1) {
        fprintf(out, "全相联没有组号。标记 = 虚页号全部 = %d 位，项数不改变这个位数\n", t->tag_bits);
    } else {
        fprintf(out, "组号 = log2(%d) = %d 位，取虚页号的低 %d 位\n", t->n_sets, t->index_bits, t->index_bits);
        fprintf(out, "标记 = %d - %d = %d 位，取虚页号的高 %d 位\n", t->vpn_bits, t->index_bits, t->tag_bits,
                t->tag_bits);
    }
    fprintf(out, "标记来自虚页号，不是页框号。页框号是 %d 位，不参加 TLB 的划分\n", g->pfn_bits);
    fprintf(out, "虚拟地址变长时，多出来的位加在虚页号的高位，也就是加在标记上，组号位数不变\n");
    fprintf(out, "结果：标记 = %d 位，组号 = %d 位\n", t->tag_bits, t->index_bits);
    return 0;
}
