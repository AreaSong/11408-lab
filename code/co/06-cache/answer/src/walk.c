#include "walk.h"

#include "cache.h"

#include <stdio.h>

static int geometry_ok(const WalkIn *in) {
    long long cells;
    if (in->rows <= 0 || in->cols <= 0 || in->elem_bytes <= 0 || in->block_bytes < in->elem_bytes) {
        fprintf(stderr, "行数、列数、元素字节都要是正数，块不能比一个元素小\n");
        return 0;
    }
    if (in->block_bytes % in->elem_bytes != 0) {
        fprintf(stderr, "元素字节要能整除块字节，否则一个元素会跨在两块上\n");
        return 0;
    }
    cells = (long long)in->rows * in->cols;
    if (cells > 1000000) {
        fprintf(stderr, "数组元素超过 1000000 个\n");
        return 0;
    }
    return 1;
}

static uint64_t elem_addr(const WalkIn *in, int row, int col) {
    return in->base + ((uint64_t)row * (uint64_t)in->cols + (uint64_t)col) * (uint64_t)in->elem_bytes;
}

static int visit(Cache *c, const WalkIn *in, uint64_t addr) {
    AccessInfo info;
    uint64_t block_bytes = (uint64_t)in->block_bytes;
    if (addr % block_bytes + (uint64_t)in->elem_bytes > block_bytes) {
        fprintf(stderr, "元素首地址 %llu 跨过块边界\n", (unsigned long long)addr);
        return 1;
    }
    if (cache_access(c, 0, addr, &info) != 0) {
        return 1;
    }
    if (in->update && cache_access(c, 1, addr, &info) != 0) {
        return 1;
    }
    return 0;
}

/* 存放永远按行优先。按列只改变访问次序，地址仍然是 base + (行×列数+列)×元素字节。 */
static int scan(Cache *c, const WalkIn *in, int col_major) {
    int row;
    int col;
    if (col_major) {
        for (col = 0; col < in->cols; col++) {
            for (row = 0; row < in->rows; row++) {
                if (visit(c, in, elem_addr(in, row, col)) != 0) {
                    return 1;
                }
            }
        }
        return 0;
    }
    for (row = 0; row < in->rows; row++) {
        for (col = 0; col < in->cols; col++) {
            if (visit(c, in, elem_addr(in, row, col)) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

int walk_run(const WalkIn *in, int col_major, WalkOut *out) {
    Fields fields;
    Cache *cache = NULL;
    CacheStat st;
    if (!geometry_ok(in)) {
        return 1;
    }
    if (fields_make(&fields, 0, in->addr_bits, in->block_bytes, in->n_lines, in->n_ways) != 0) {
        return 1;
    }
    if (cache_open(&cache, &fields, in->replace, in->write_back, in->write_alloc) != 0) {
        return 1;
    }
    if (scan(cache, in, col_major) != 0) {
        cache_close(cache);
        return 1;
    }
    cache_stat(cache, &st);
    cache_close(cache);
    out->hits = st.hits;
    out->misses = st.misses;
    out->accesses = st.hits + st.misses;
    return 0;
}

static void show_intro(FILE *out, const WalkIn *in, const Fields *f) {
    int per = in->block_bytes / in->elem_bytes;
    fprintf(out, "【数组访问】\n");
    if (in->base == 0) {
        fprintf(out, "数组 %d 行 × %d 列，元素 %d B，按行优先存放，首地址 0\n", in->rows, in->cols, in->elem_bytes);
    } else {
        fprintf(out, "数组 %d 行 × %d 列，元素 %d B，按行优先存放，首地址 %llu = %llXH\n", in->rows, in->cols,
                in->elem_bytes, (unsigned long long)in->base, (unsigned long long)in->base);
    }
    fprintf(out, "按字节编址，主存地址 %d 位，块 %d B，Cache %d 行，每组 %d 路，%s\n", in->addr_bits, in->block_bytes,
            in->n_lines, in->n_ways, fields_map_name(f));
    fprintf(out, "一块装 %d 个元素。%s。Cache 从空开始\n", per,
            in->update ? "每个元素先读后写，算 2 次访问" : "每个元素读 1 次");
}

static void show_row_reason(FILE *out, const WalkIn *in) {
    long long bytes = (long long)in->rows * in->cols * in->elem_bytes;
    int per = in->block_bytes / in->elem_bytes;
    fprintf(out, "【按行】\n");
    if (in->base % (uint64_t)in->block_bytes == 0 && bytes % in->block_bytes == 0) {
        fprintf(out, "按行相邻元素相差 %d B，%d 个元素刚好一块，整段地址对齐到块\n", in->elem_bytes, per);
        fprintf(out, "每块只在第一次被读到时缺失，块里其余访问命中\n");
    } else {
        fprintf(out, "首地址或总字节数没有对齐到块，命中次数按逐个元素模拟\n");
    }
}

static void show_col_reason(FILE *out, const WalkIn *in, const Fields *f) {
    unsigned long long stride = (unsigned long long)in->cols * (unsigned long long)in->elem_bytes;
    fprintf(out, "【按列】\n");
    if (in->rows == 1) {
        fprintf(out, "只有一行，按列和按行走到的地址相同\n");
        return;
    }
    fprintf(out, "同一列相邻两行相差 %llu B\n", stride);
    if (stride % (unsigned long long)in->block_bytes != 0) {
        fprintf(out, "这个间距不是块的整数倍\n");
        return;
    }
    fprintf(out, "合 %llu 个主存块，对组数 %d 取模得 %llu\n", stride / (unsigned long long)in->block_bytes, f->n_sets,
            (stride / (unsigned long long)in->block_bytes) % (unsigned long long)f->n_sets);
    if ((stride / (unsigned long long)in->block_bytes) % (unsigned long long)f->n_sets == 0 && f->n_ways == 1) {
        fprintf(out, "整列进同一组，组内只有 1 路，相邻行不能同时留在 Cache 里\n");
    }
}

static void show_result(FILE *out, const WalkOut *st) {
    char hit_pct[32];
    char miss_pct[32];
    format_percent(hit_pct, sizeof hit_pct, st->hits, st->accesses);
    format_percent(miss_pct, sizeof miss_pct, st->misses, st->accesses);
    fprintf(out, "结果：访问 %d 次，命中 %d 次，缺失 %d 次，命中率 = %s，缺失率 = %s\n", st->accesses, st->hits,
            st->misses, hit_pct, miss_pct);
}

static int show_one(FILE *out, const WalkIn *in, const Fields *f, int col_major) {
    WalkOut st;
    if (col_major) {
        show_col_reason(out, in, f);
    } else {
        show_row_reason(out, in);
    }
    if (walk_run(in, col_major, &st) != 0) {
        return 1;
    }
    show_result(out, &st);
    return 0;
}

int show_walk(FILE *out, const WalkIn *in, int which) {
    Fields fields;
    if (!geometry_ok(in) || fields_make(&fields, 0, in->addr_bits, in->block_bytes, in->n_lines, in->n_ways) != 0) {
        return 1;
    }
    show_intro(out, in, &fields);
    if (which != WALK_COL && show_one(out, in, &fields, 0) != 0) {
        return 1;
    }
    if (which != WALK_ROW && show_one(out, in, &fields, 1) != 0) {
        return 1;
    }
    return 0;
}
