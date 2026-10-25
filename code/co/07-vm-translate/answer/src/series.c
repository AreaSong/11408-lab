#include "series.h"

#include "cache.h"
#include "fields.h"
#include "geom.h"

#include <stdio.h>

static int open_tlb(Cache **out, int vpn_bits, int n_lines, int n_ways) {
    Fields fields;
    if (fields_make(&fields, 0, vpn_bits, 1, n_lines, n_ways) != 0) {
        return 1;
    }
    /* 只读虚页号。写策略用不上，写直达加写分配不会在这条路径里写主存。 */
    return cache_open(out, &fields, REPLACE_LRU, 0, 1);
}

int series_victims(int vpn_bits, int n_lines, int n_ways, const uint64_t *vpns, int n, uint64_t *victims,
                   int *n_victims, int *hits) {
    Cache *cache = NULL;
    int i;
    if (open_tlb(&cache, vpn_bits, n_lines, n_ways) != 0) {
        return 1;
    }
    *n_victims = 0;
    *hits = 0;
    for (i = 0; i < n; i++) {
        AccessInfo info;
        if (cache_access(cache, 0, vpns[i], &info) != 0) {
            cache_close(cache);
            return 1;
        }
        if (info.hit) {
            (*hits)++;
        }
        if (info.replaced) {
            victims[(*n_victims)++] = info.old_block;
        }
    }
    cache_close(cache);
    return 0;
}

static void show_step(FILE *out, int index, uint64_t vpn, const TlbGeom *tlb, const AccessInfo *info) {
    int set = 0;
    uint64_t tag = 0;
    if (tlb_parts(tlb, vpn, &set, &tag) != 0) {
        return;
    }
    (void)tag;
    fprintf(out, "%d  虚页号 %llu  组号 %d  标记 %llu  ", index, (unsigned long long)vpn, info->set,
            (unsigned long long)info->tag);
    if (info->hit) {
        fprintf(out, "命中  路 %d\n", info->way);
    } else if (info->replaced) {
        fprintf(out, "缺失  替换虚页号 %llu\n", (unsigned long long)info->old_block);
    } else {
        fprintf(out, "缺失  装入空行，路 %d\n", info->way);
    }
    if (set != info->set) {
        fprintf(out, "   组号和虚页号 mod 组数不一致\n");
    }
}

static void show_victims(FILE *out, const uint64_t *victims, int n_victims, int hits) {
    int i;
    fprintf(out, "结果：命中 %d 次，", hits);
    if (n_victims == 0) {
        fprintf(out, "没有替换\n");
        return;
    }
    if (n_victims == 1) {
        fprintf(out, "被替换的虚页号 = %llu\n", (unsigned long long)victims[0]);
        return;
    }
    fprintf(out, "被替换的虚页号依次是");
    for (i = 0; i < n_victims; i++) {
        fprintf(out, "%s %llu", i == 0 ? "" : "、", (unsigned long long)victims[i]);
    }
    fprintf(out, "\n");
}

int show_series(FILE *out, int va_bits, int page_bytes, int n_lines, int n_ways, const uint64_t *vpns, int n) {
    PageGeom page;
    TlbGeom tlb;
    Cache *cache = NULL;
    uint64_t victims[64];
    int n_victims = 0;
    int hits = 0;
    int i;
    if (page_geom(&page, va_bits, va_bits, page_bytes) != 0 || tlb_geom(&tlb, page.vpn_bits, n_lines, n_ways) != 0) {
        return 1;
    }
    if (open_tlb(&cache, page.vpn_bits, n_lines, n_ways) != 0) {
        return 1;
    }
    fprintf(out, "【TLB 替换】\n");
    fprintf(out, "页 %d B，虚页号 %d 位。页内偏移不进 TLB\n", page_bytes, page.vpn_bits);
    fprintf(out, "把虚页号当成地址、块大小当成 1，组相联和第 06 关的 Cache 是同一套\n");
    fprintf(out, "%d 项，每组 %d 路，%d 组。组号 = 虚页号 mod %d，也就是虚页号的低 %d 位\n", n_lines, n_ways,
            tlb.n_sets, tlb.n_sets, tlb.index_bits);
    fprintf(out, "LRU：命中或刚装入的项是最新的，替换同一组里最久没访问的那一项。起初 TLB 是空的\n");
    for (i = 0; i < n; i++) {
        AccessInfo info;
        if (vpns[i] > geom_mask(page.vpn_bits) || cache_access(cache, 0, vpns[i], &info) != 0) {
            fprintf(stderr, "第 %d 个虚页号无法装进 %d 位\n", i + 1, page.vpn_bits);
            cache_close(cache);
            return 1;
        }
        show_step(out, i + 1, vpns[i], &tlb, &info);
        if (info.hit) {
            hits++;
        }
        if (info.replaced) {
            if (n_victims >= (int)(sizeof victims / sizeof victims[0])) {
                fprintf(stderr, "替换次数超过 %d\n", n_victims);
                cache_close(cache);
                return 1;
            }
            victims[n_victims++] = info.old_block;
        }
    }
    cache_close(cache);
    show_victims(out, victims, n_victims, hits);
    return 0;
}
