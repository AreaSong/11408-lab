#include "map.h"

#include "fmt.h"

#include <stdio.h>

int cache_fields(Fields *out, int pa_bits, int n_lines, int block, int n_ways) {
    return fields_make(out, 0, pa_bits, block, n_lines, n_ways);
}

int cache_from_bytes(Fields *out, int pa_bits, int cache_bytes, int block, int n_ways) {
    int lines;
    if (block <= 0 || cache_bytes <= 0 || cache_bytes % block != 0) {
        fprintf(stderr, "Cache 数据区要能被块大小整除\n");
        return 1;
    }
    lines = cache_bytes / block;
    return fields_make(out, 0, pa_bits, block, lines, n_ways);
}

int block_place(const Fields *f, uint64_t block_no, int *set, uint64_t *tag) {
    int block_bits = f->addr_bits - f->offset_bits;
    if (block_bits < f->index_bits || block_no > geom_mask(block_bits)) {
        fprintf(stderr, "块号超出物理地址能表示的范围\n");
        return 1;
    }
    *set = (int)(block_no & geom_mask(f->index_bits));
    *tag = f->index_bits == 0 ? block_no : (block_no >> f->index_bits);
    return 0;
}

int show_cfield(FILE *out, const Fields *f) {
    fprintf(out, "【Cache 字段】\n");
    fprintf(out, "按字节编址，物理地址 %d 位\n", f->addr_bits);
    fprintf(out, "%d 行，每组 %d 路，%s，块 %d B\n", f->n_lines, f->n_ways, fields_map_name(f), f->block_units);
    fprintf(out, "块内偏移 = log2(%d) = %d 位，在物理地址的最低位\n", f->block_units, f->offset_bits);
    fprintf(out, "组号 = log2(%d) = %d 位，紧挨在块内偏移上面\n", f->n_sets, f->index_bits);
    if (f->n_ways <= 1) {
        fprintf(out, "直接映射时组号就是行号\n");
    }
    fprintf(out, "标记 = %d - %d - %d = %d 位，在最高位\n", f->addr_bits, f->index_bits, f->offset_bits, f->tag_bits);
    fprintf(out, "结果：标记 = %d 位，组号 = %d 位，块内偏移 = %d 位\n", f->tag_bits, f->index_bits, f->offset_bits);
    return 0;
}

int show_band(FILE *out, const PageGeom *g, const TlbGeom *t, const Fields *f) {
    fprintf(out, "【地址字段】\n");
    fprintf(out, "按字节编址，虚拟地址 %d 位，物理地址 %d 位，页 %d B\n", g->va_bits, g->pa_bits, g->page_bytes);
    fprintf(out, "A 虚页号 = %d - %d = %d 位\n", g->va_bits, g->offset_bits, g->vpn_bits);
    fprintf(out, "D 页内偏移 = log2(%d) = %d 位\n", g->page_bytes, g->offset_bits);
    fprintf(out, "C 页框号 = %d - %d = %d 位\n", g->pa_bits, g->offset_bits, g->pfn_bits);
    if (t->n_sets <= 1) {
        fprintf(out, "TLB %d 项全相联。标记就是整个虚页号，项数不改变标记有多少位\n", t->n_lines);
    } else {
        fprintf(out, "TLB %d 项，每组 %d 路，组号 %d 位\n", t->n_lines, t->n_ways, t->index_bits);
    }
    fprintf(out, "B TLB 标记 = %d 位，里面放的是虚页号里用来比较的那一段\n", t->tag_bits);
    fprintf(out, "Cache 数据区 %lld B，块 %d B，%d 行，每组 %d 路，%s\n",
            (long long)f->n_lines * f->block_units, f->block_units, f->n_lines, f->n_ways, fields_map_name(f));
    fprintf(out, "行数 = %lld / %d = %d，组数 = %d / %d = %d\n", (long long)f->n_lines * f->block_units,
            f->block_units, f->n_lines, f->n_lines, f->n_ways, f->n_sets);
    fprintf(out, "G 块内偏移 = log2(%d) = %d 位\n", f->block_units, f->offset_bits);
    fprintf(out, "F 组号 = log2(%d) = %d 位\n", f->n_sets, f->index_bits);
    fprintf(out, "E Cache 标记 = %d - %d - %d = %d 位\n", f->addr_bits, f->index_bits, f->offset_bits, f->tag_bits);
    fprintf(out, "结果：A = %d 位，B = %d 位，C = %d 位，D = %d 位，E = %d 位，F = %d 位，G = %d 位\n", g->vpn_bits,
            t->tag_bits, g->pfn_bits, g->offset_bits, f->tag_bits, f->index_bits, f->offset_bits);
    return 0;
}

int show_blk(FILE *out, const Fields *f, uint64_t block_no) {
    int set = 0;
    uint64_t tag = 0;
    int block_bits = f->addr_bits - f->offset_bits;
    char block_bin[80];
    char set_bin[80];
    char tag_bin[80];
    char tag_hex[32];
    if (block_place(f, block_no, &set, &tag) != 0) {
        return 1;
    }
    fmt_bin(block_bin, sizeof block_bin, block_no, block_bits);
    fmt_bin(set_bin, sizeof set_bin, (uint64_t)set, f->index_bits);
    fmt_bin(tag_bin, sizeof tag_bin, tag, f->tag_bits);
    fmt_hex(tag_hex, sizeof tag_hex, tag, f->tag_bits);
    fprintf(out, "【主存块进 Cache】\n");
    fprintf(out, "按字节编址，物理地址 %d 位，块 %d B，%d 行，每组 %d 路，%s\n", f->addr_bits, f->block_units,
            f->n_lines, f->n_ways, fields_map_name(f));
    fprintf(out, "块号位数 = %d - %d = %d 位\n", f->addr_bits, f->offset_bits, block_bits);
    fprintf(out, "块号 %llu = %s\n", (unsigned long long)block_no, block_bin);
    fprintf(out, "组号 = 块号的低 %d 位 = %s = %d\n", f->index_bits, set_bin, set);
    fprintf(out, "标记 = 块号的高 %d 位 = %s = %s\n", f->tag_bits, tag_bin, tag_hex);
    fprintf(out, "组号也可以写成块号 mod 组数：%llu mod %d = %d\n", (unsigned long long)block_no, f->n_sets, set);
    fprintf(out, "结果：组号 = %d，标记 = %s\n", set, tag_bin);
    return 0;
}

int show_cset(FILE *out, const PageGeom *page, const Fields *f, uint64_t addr, int low_only) {
    uint64_t block = 0;
    uint64_t tag = 0;
    uint64_t offset = 0;
    uint64_t used = addr;
    int set = 0;
    int span = f->offset_bits + f->index_bits;
    char addr_text[32];
    if (low_only) {
        if (span > page->offset_bits) {
            fprintf(stderr, "组号和块内偏移已经超出页内偏移，必须先得到页框号\n");
            return 1;
        }
        used = addr & geom_mask(span);
    }
    if (fields_locate(f, low_only ? used : addr, &block, &set, &tag, &offset) != 0) {
        return 1;
    }
    fmt_hex(addr_text, sizeof addr_text, addr, 0);
    fprintf(out, "【Cache 组号】\n");
    fprintf(out, "页 %d B，页内偏移 %d 位\n", page->page_bytes, page->offset_bits);
    fprintf(out, "Cache 块内偏移 %d 位，组号 %d 位，一共 %d 位\n", f->offset_bits, f->index_bits, span);
    if (span <= page->offset_bits) {
        fprintf(out, "这 %d 位落在页内偏移里。虚地址和物理地址的这几位相同，可以不先翻译页框号\n", span);
    }
    fprintf(out, "地址 %s 的组号 = %d，块内偏移 = %llu\n", addr_text, set, (unsigned long long)offset);
    if (low_only) {
        fprintf(out, "标记还要看页框号。这一问只取组号\n");
        fprintf(out, "结果：组号 = %d\n", set);
        return 0;
    }
    fprintf(out, "结果：组号 = %d，标记 = %llu，块内偏移 = %llu\n", set, (unsigned long long)tag,
            (unsigned long long)offset);
    return 0;
}
