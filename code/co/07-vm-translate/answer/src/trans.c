#include "trans.h"

#include "fmt.h"

#include <stdio.h>
#include <string.h>

static int find_pte(const PteItem *ptes, int n_pte, uint64_t vpn, int *present, uint64_t *frame) {
    int i;
    int found = 0;
    for (i = 0; i < n_pte; i++) {
        if (ptes[i].vpn != vpn) {
            continue;
        }
        if (found) {
            fprintf(stderr, "虚页号 %llu 在页表里出现了两次\n", (unsigned long long)vpn);
            return -1;
        }
        *present = ptes[i].present;
        *frame = ptes[i].frame;
        found = 1;
    }
    return found;
}

/* 命中返回 1，没有返回 0，同一组两项同时命中返回 -1。 */
static int find_tlb(const TlbGeom *tlb, const TlbSlot *slots, int set, uint64_t tag, int *way, uint64_t *frame) {
    int i;
    int hit = -1;
    for (i = 0; i < tlb->n_ways; i++) {
        const TlbSlot *slot = &slots[set * tlb->n_ways + i];
        if (!slot->given || !slot->valid || slot->tag != tag) {
            continue;
        }
        if (hit >= 0) {
            fprintf(stderr, "TLB 第 %d 组有两项同时命中\n", set);
            return -1;
        }
        hit = i;
        *frame = slot->frame;
    }
    *way = hit;
    return hit >= 0;
}

static int probe_cache(const Fields *cache, const LineSlot *lines, uint64_t pa, TransOut *out) {
    uint64_t block = 0;
    int i;
    if (fields_locate(cache, pa, &block, &out->cache_set, &out->cache_tag, &out->cache_off) != 0) {
        return 1;
    }
    out->cache_on = 1;
    out->cache_hit = 0;
    out->cache_way = -1;
    out->invalid_tag = 0;
    for (i = 0; i < cache->n_ways; i++) {
        const LineSlot *line = &lines[out->cache_set * cache->n_ways + i];
        if (!line->given || line->tag != out->cache_tag) {
            continue;
        }
        if (line->valid) {
            out->cache_hit = 1;
            out->cache_way = i;
        } else {
            out->invalid_tag = 1;
        }
    }
    return 0;
}

static int finish_pa(const TransIn *in, TransOut *out) {
    if (pa_join(in->page, out->frame, out->offset, &out->pa) != 0) {
        return 1;
    }
    if (in->cache == NULL) {
        return 0;
    }
    return probe_cache(in->cache, in->lines, out->pa, out);
}

int trans_run(const TransIn *in, TransOut *out) {
    int present = 0;
    int found;
    int hit;
    memset(out, 0, sizeof(*out));
    out->tlb_way = -1;
    out->cache_way = -1;
    if (va_split(in->page, in->va, &out->vpn, &out->offset) != 0) {
        return 1;
    }
    if (in->tlb != NULL) {
        if (tlb_parts(in->tlb, out->vpn, &out->tlb_set, &out->tlb_tag) != 0) {
            return 1;
        }
        hit = find_tlb(in->tlb, in->slots, out->tlb_set, out->tlb_tag, &out->tlb_way, &out->frame);
        if (hit < 0) {
            return 1;
        }
        if (hit == 1) {
            out->via = VIA_TLB;
            return finish_pa(in, out);
        }
    }
    found = find_pte(in->ptes, in->n_pte, out->vpn, &present, &out->frame);
    if (found < 0) {
        return 1;
    }
    if (!found) {
        out->via = VIA_NO_PTE;
        return 0;
    }
    if (!present) {
        out->via = VIA_FAULT;
        return 0;
    }
    out->via = VIA_PTE;
    return finish_pa(in, out);
}

static void show_split(FILE *out, const PageGeom *g, const TransOut *r, uint64_t va) {
    char va_text[32];
    char vpn_text[32];
    char off_text[32];
    fmt_hex(va_text, sizeof va_text, va, g->va_bits);
    fmt_hex(vpn_text, sizeof vpn_text, r->vpn, g->vpn_bits);
    fmt_hex(off_text, sizeof off_text, r->offset, g->offset_bits);
    fprintf(out, "按字节编址，虚拟地址 %d 位，物理地址 %d 位，页 %d B\n", g->va_bits, g->pa_bits, g->page_bytes);
    fprintf(out, "页内偏移 %d 位，虚页号 %d 位，页框号 %d 位\n", g->offset_bits, g->vpn_bits, g->pfn_bits);
    fprintf(out, "虚拟地址 %s\n", va_text);
    fprintf(out, "虚页号 = 高 %d 位 = %s = %llu\n", g->vpn_bits, vpn_text, (unsigned long long)r->vpn);
    fprintf(out, "页内偏移 = 低 %d 位 = %s\n", g->offset_bits, off_text);
    fprintf(out, "页内偏移不参加比较，翻译完原样拼到页框号后面\n");
}

static void show_slot(FILE *out, const PageGeom *g, const TlbGeom *tlb, const TlbSlot *slot, int way, uint64_t tag) {
    char tag_text[32];
    char frame_text[32];
    if (!slot->given) {
        fprintf(out, "  路%d 空\n", way);
        return;
    }
    fmt_hex(tag_text, sizeof tag_text, slot->tag, tlb->tag_bits);
    fmt_hex(frame_text, sizeof frame_text, slot->frame, g->pfn_bits);
    fprintf(out, "  路%d 有效位 %d 标记 %s 页框号 %s  ", way, slot->valid, tag_text, frame_text);
    if (!slot->valid && slot->tag == tag) {
        fprintf(out, "标记相同，但有效位是 0，不能算命中\n");
    } else if (!slot->valid) {
        fprintf(out, "有效位是 0\n");
    } else if (slot->tag == tag) {
        fprintf(out, "命中\n");
    } else {
        fprintf(out, "标记不同\n");
    }
}

static void show_tlb_set(FILE *out, const PageGeom *g, const TlbGeom *tlb, const TlbSlot *slots, const TransOut *r) {
    int way;
    char tag_text[32];
    fmt_hex(tag_text, sizeof tag_text, r->tlb_tag, tlb->tag_bits);
    fprintf(out, "TLB %d 项，每组 %d 路，%s\n", tlb->n_lines, tlb->n_ways,
            tlb->n_sets <= 1 ? "全相联" : "组相联");
    if (tlb->n_sets <= 1) {
        fprintf(out, "全相联没有组号，标记 = 整个虚页号 = %s\n", tag_text);
    } else {
        fprintf(out, "组号 = 虚页号的低 %d 位 = %d，标记 = %s\n", tlb->index_bits, r->tlb_set, tag_text);
    }
    for (way = 0; way < tlb->n_ways; way++) {
        show_slot(out, g, tlb, &slots[r->tlb_set * tlb->n_ways + way], way, r->tlb_tag);
    }
    fprintf(out, "有效位是 0 只说明这项空着。缺页要等页表的存在位说了才算\n");
}

static void show_rows(FILE *out, const PageGeom *g, const PteItem *ptes, int n_pte, uint64_t vpn) {
    int i;
    fprintf(out, "页表按虚页号查。没写 H、也没写 0x 的数是十进制\n");
    for (i = 0; i < n_pte; i++) {
        char own[32];
        char frame[32];
        int same = ptes[i].vpn == vpn;
        fmt_hex(own, sizeof own, ptes[i].vpn, g->vpn_bits);
        fmt_hex(frame, sizeof frame, ptes[i].frame, g->pfn_bits);
        fprintf(out, "  虚页号 %llu（%s） 存在位 %d ", (unsigned long long)ptes[i].vpn, own, ptes[i].present);
        if (ptes[i].present) {
            fprintf(out, "页框号 %s", frame);
        } else {
            fprintf(out, "页框号无效");
        }
        if (!same) {
            fprintf(out, "  不是这一项\n");
        } else if (!ptes[i].present) {
            fprintf(out, "  就是这一项，存在位是 0\n");
        } else {
            fprintf(out, "  命中\n");
        }
    }
}

static void show_pa(FILE *out, const PageGeom *g, const TransOut *r) {
    char frame[32];
    char off[32];
    char pa[32];
    char pa_h[32];
    fmt_hex(frame, sizeof frame, r->frame, g->pfn_bits);
    fmt_hex(off, sizeof off, r->offset, g->offset_bits);
    fmt_hex(pa, sizeof pa, r->pa, g->pa_bits);
    fmt_h(pa_h, sizeof pa_h, r->pa, g->pa_bits);
    fprintf(out, "物理地址 = 页框号 %s 拼上页内偏移 %s = %s（%s）\n", frame, off, pa, pa_h);
}

static void show_line(FILE *out, const Fields *cache, const LineSlot *line, int way, uint64_t tag) {
    char text[32];
    if (!line->given) {
        fprintf(out, "  路%d 空\n", way);
        return;
    }
    fmt_hex(text, sizeof text, line->tag, cache->tag_bits);
    fprintf(out, "  路%d 有效位 %d 标记 %s  ", way, line->valid, text);
    if (line->tag != tag) {
        fprintf(out, "标记不同\n");
    } else if (!line->valid) {
        fprintf(out, "标记相同，有效位是 0\n");
    } else {
        fprintf(out, "命中\n");
    }
}

static void show_cache(FILE *out, const Fields *cache, const LineSlot *lines, const TransOut *r) {
    int way;
    char tag[32];
    char off[32];
    fmt_hex(tag, sizeof tag, r->cache_tag, cache->tag_bits);
    fmt_hex(off, sizeof off, r->cache_off, cache->offset_bits == 0 ? 1 : cache->offset_bits);
    fprintf(out, "物理地址再进 Cache。字段划分复用第 06 关：标记、组号、块内偏移\n");
    fprintf(out, "%s，%d 行，每组 %d 路，块 %d B\n", fields_map_name(cache), cache->n_lines, cache->n_ways,
            cache->block_units);
    fprintf(out, "标记 %s，组号 %d，块内偏移 %s\n", tag, r->cache_set, off);
    if (cache->n_ways <= 1) {
        fprintf(out, "直接映射，组号就是行号\n");
    }
    for (way = 0; way < cache->n_ways; way++) {
        show_line(out, cache, &lines[r->cache_set * cache->n_ways + way], way, r->cache_tag);
    }
    if (r->cache_hit) {
        fprintf(out, "标记相同而且有效位是 1，数据从 Cache 取\n");
        return;
    }
    if (r->invalid_tag) {
        fprintf(out, "标记对上的那一行有效位是 0，这一行不能用，所以 Cache 缺失\n");
    } else {
        fprintf(out, "这一组里没有标记相同的有效行，Cache 缺失\n");
    }
    fprintf(out, "Cache 缺失只说明这块不在 Cache 里。页已经在主存，这不是缺页\n");
}

static void show_verdict(FILE *out, const PageGeom *g, const TransIn *in, const TransOut *r) {
    char pa[32];
    char pa_h[32];
    char vpn[32];
    fmt_hex(pa, sizeof pa, r->pa, g->pa_bits);
    fmt_h(pa_h, sizeof pa_h, r->pa, g->pa_bits);
    fmt_hex(vpn, sizeof vpn, r->vpn, g->vpn_bits);
    fprintf(out, "结果：");
    if (r->via == VIA_FAULT) {
        fprintf(out, "缺页\n");
        return;
    }
    if (r->via == VIA_NO_PTE) {
        if (in->tlb != NULL) {
            fprintf(out, "TLB 缺失，页表未给出虚页号 %s\n", vpn);
        } else {
            fprintf(out, "页表未给出虚页号 %s\n", vpn);
        }
        return;
    }
    if (r->via == VIA_TLB) {
        fprintf(out, "TLB 命中，物理地址 = %s（%s）", pa, pa_h);
    } else if (in->tlb != NULL) {
        fprintf(out, "TLB 缺失，页表命中，物理地址 = %s（%s）", pa, pa_h);
    } else {
        fprintf(out, "页表命中，物理地址 = %s（%s）", pa, pa_h);
    }
    if (r->cache_on) {
        fprintf(out, "，Cache %s", r->cache_hit ? "命中" : "缺失");
    }
    fprintf(out, "\n");
}

static void show_after_lookup(FILE *out, const TransIn *in, const TransOut *r) {
    if (r->via == VIA_TLB) {
        fprintf(out, "快表命中，页框号从这一项直接拿，不再访问主存里的页表\n");
        return;
    }
    if (in->tlb != NULL) {
        fprintf(out, "快表没有命中，改查主存里的页表\n");
    } else {
        fprintf(out, "题目没有给 TLB，直接查页表\n");
    }
    show_rows(out, in->page, in->ptes, in->n_pte, r->vpn);
    if (r->via == VIA_FAULT) {
        fprintf(out, "存在位是 0：这一页不在主存。地址翻译停在这里，不拼物理地址，也不查 Cache\n");
        fprintf(out, "缺页交给操作系统调页，处理完后重新执行这条访存\n");
        return;
    }
    if (r->via == VIA_NO_PTE) {
        fprintf(out, "给出的页表里没有这个虚页号，不能把它当成缺页，也不能当成命中\n");
        return;
    }
    fprintf(out, "存在位是 1，页在主存。硬件会把这项写入 TLB，下次同一虚页不必再查页表\n");
}

static int show_body(FILE *out, const char *title, const TransIn *in) {
    TransOut result;
    if (trans_run(in, &result) != 0) {
        return 1;
    }
    fprintf(out, "【%s】\n", title);
    show_split(out, in->page, &result, in->va);
    if (in->tlb != NULL) {
        show_tlb_set(out, in->page, in->tlb, in->slots, &result);
    }
    show_after_lookup(out, in, &result);
    if (result.via == VIA_TLB || result.via == VIA_PTE) {
        show_pa(out, in->page, &result);
        if (in->cache != NULL) {
            show_cache(out, in->cache, in->lines, &result);
        } else {
            fprintf(out, "本题不查 Cache\n");
        }
    }
    show_verdict(out, in->page, in, &result);
    return 0;
}

int show_access(FILE *out, const TransIn *in) {
    return show_body(out, "一次访存", in);
}

int show_pt(FILE *out, const PageGeom *g, const PteItem *ptes, int n_pte, uint64_t va) {
    TransIn in;
    memset(&in, 0, sizeof in);
    in.page = g;
    in.ptes = ptes;
    in.n_pte = n_pte;
    in.va = va;
    return show_body(out, "页表查找", &in);
}
