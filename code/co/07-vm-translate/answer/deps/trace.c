#include "trace.h"

static void show_set(FILE *out, const Cache *c, int set, int star_way) {
    int way;
    int ways = cache_ways(c);
    fprintf(out, "   组 %d：", set);
    for (way = 0; way < ways; way++) {
        int valid = 0;
        int dirty = 0;
        uint64_t block = 0;
        cache_peek(c, set, way, &valid, &dirty, &block);
        if (way > 0) {
            fprintf(out, "    ");
        }
        if (!valid) {
            fprintf(out, "路%d = 空", way);
            continue;
        }
        fprintf(out, "路%d = 块 %llu%s%s", way, (unsigned long long)block, dirty ? " 脏" : "",
                way == star_way ? "*" : "");
    }
    fprintf(out, "\n");
}

int trace_show_header(FILE *out, const Fields *f, int replace, int write_back, int write_alloc) {
    const char *unit = f->word ? "字" : "B";
    fprintf(out, "【Cache 访问】\n");
    fprintf(out, "按%s编址，主存地址 %d 位，块 %d %s，%d 行，每组 %d 路，%s\n", f->word ? "字" : "字节",
            f->addr_bits, f->block_units, unit, f->n_lines, f->n_ways, fields_map_name(f));
    fprintf(out, "组数 = %d，块内偏移 %d 位，组号 %d 位，标记 %d 位\n", f->n_sets, f->offset_bits, f->index_bits,
            f->tag_bits);
    if (replace == REPLACE_LRU) {
        fprintf(out, "LRU：命中或刚装入的行是最新的，替换最久没碰的那一行\n");
    } else {
        fprintf(out, "FIFO：按装入次序替换，命中不改变次序\n");
    }
    fprintf(out, "写命中采用%s，写缺失采用%s\n", write_back ? "写回" : "写直达",
            write_alloc ? "写分配" : "非写分配");
    fprintf(out, "写回是脏块被换出；当时写主存是写直达或非写分配立刻写进主存的次数\n");
    fprintf(out, "快照里 * 是本次访问的路，脏表示写回法下这一行比主存新\n");
    return 0;
}

int trace_show_access(FILE *out, int index, int is_write, uint64_t addr, const Cache *c,
                      const AccessInfo *info) {
    fprintf(out, "%d  %s  地址 %llu  块号 %llu  组号 %d  标记 %llu  %s", index, is_write ? "写" : "读",
            (unsigned long long)addr, (unsigned long long)info->block, info->set,
            (unsigned long long)info->tag, info->hit ? "命中" : "缺失");
    if (info->hit) {
        fprintf(out, "  路 %d\n", info->way);
    } else if (info->bypass) {
        fprintf(out, "  不调块，直接写主存\n");
    } else if (info->replaced && info->writeback) {
        fprintf(out, "  替换路 %d 的块 %llu，写回主存\n", info->way, (unsigned long long)info->old_block);
    } else if (info->replaced) {
        fprintf(out, "  替换路 %d 的块 %llu\n", info->way, (unsigned long long)info->old_block);
    } else {
        fprintf(out, "  装入空行，路 %d\n", info->way);
    }
    show_set(out, c, info->set, info->bypass ? -1 : info->way);
    return 0;
}

int trace_show_summary(FILE *out, const Cache *c) {
    CacheStat st;
    char hit_pct[32];
    char miss_pct[32];
    int total;
    cache_stat(c, &st);
    total = st.hits + st.misses;
    format_percent(hit_pct, sizeof hit_pct, st.hits, total);
    format_percent(miss_pct, sizeof miss_pct, st.misses, total);
    fprintf(out, "结果：命中 %d 次，缺失 %d 次，命中率 = %s，缺失率 = %s，调入 %d 块，写回 %d 次，当时写主存 %d 次\n",
            st.hits, st.misses, hit_pct, miss_pct, st.fills, st.writebacks, st.mem_writes);
    return 0;
}
