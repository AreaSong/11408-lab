#include "fields.h"

#include <stdio.h>
#include <string.h>

static int is_pow2(int x) {
    return x > 0 && (x & (x - 1)) == 0;
}

static int log2_pow2(int x) {
    int n = 0;
    while (x > 1) {
        x >>= 1;
        n++;
    }
    return n;
}

const char *fields_map_name(const Fields *f) {
    if (f->n_ways <= 1) {
        return "直接映射";
    }
    if (f->n_sets <= 1) {
        return "全相联";
    }
    return "组相联";
}

void format_percent(char *dst, size_t n, int part, int total) {
    long long scaled;
    long long whole;
    int frac;
    if (n == 0) {
        return;
    }
    if (total <= 0) {
        snprintf(dst, n, "0%%");
        return;
    }
    /* 93.75% 这种两位小数优先。96.875% 要三位才除得尽。再不行就写成分数。 */
    if ((long long)part * 10000 % total != 0) {
        long long fine;
        long long whole3;
        int frac3;
        if ((long long)part * 100000 % total != 0) {
            snprintf(dst, n, "%d/%d", part, total);
            return;
        }
        fine = (long long)part * 100000 / total;
        whole3 = fine / 1000;
        frac3 = (int)(fine % 1000);
        snprintf(dst, n, "%lld.%03d%%", whole3, frac3);
        return;
    }
    scaled = (long long)part * 10000 / total;
    whole = scaled / 100;
    frac = (int)(scaled % 100);
    if (frac == 0) {
        snprintf(dst, n, "%lld%%", whole);
    } else if (frac % 10 == 0) {
        snprintf(dst, n, "%lld.%d%%", whole, frac / 10);
    } else {
        snprintf(dst, n, "%lld.%02d%%", whole, frac);
    }
}

int fields_make(Fields *out, int word, int addr_bits, int block_units, int n_lines, int n_ways) {
    if (out == NULL) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    if (!is_pow2(block_units) || !is_pow2(n_lines) || !is_pow2(n_ways) || n_ways > n_lines ||
        n_lines > (1 << 20) || block_units > (1 << 20)) {
        fprintf(stderr, "块大小、行数、路数都要是 2 的幂，路数不能大于行数，行数不超过 2^20\n");
        return 1;
    }
    out->word = word ? 1 : 0;
    out->block_units = block_units;
    out->n_lines = n_lines;
    out->n_ways = n_ways;
    out->n_sets = n_lines / n_ways;
    out->offset_bits = log2_pow2(block_units);
    out->index_bits = log2_pow2(out->n_sets);
    if (addr_bits == 0) {
        out->addr_bits = 0;
        out->tag_bits = -1;
        return 0;
    }
    if (addr_bits < out->offset_bits + out->index_bits || addr_bits > 62) {
        fprintf(stderr, "地址位数要能放下块内偏移和组号，并且不超过 62\n");
        return 1;
    }
    out->addr_bits = addr_bits;
    out->tag_bits = addr_bits - out->offset_bits - out->index_bits;
    return 0;
}

int fields_locate(const Fields *f, uint64_t addr, uint64_t *block, int *set, uint64_t *tag,
                  uint64_t *offset) {
    uint64_t b;
    if (f->addr_bits > 0 && addr >= (1ull << f->addr_bits)) {
        fprintf(stderr, "地址 %llu 超出 %d 位主存地址\n", (unsigned long long)addr, f->addr_bits);
        return 1;
    }
    if (f->offset_bits == 0) {
        b = addr;
        *offset = 0;
    } else {
        b = addr >> f->offset_bits;
        *offset = addr & ((1ull << f->offset_bits) - 1);
    }
    *block = b;
    *set = (int)(b & (uint64_t)(f->n_sets - 1));
    *tag = b >> f->index_bits;
    return 0;
}

int capacity_make(Capacity *out, const Fields *f, int write_back, int replace) {
    int meta;
    if (f->word || f->tag_bits < 0) {
        fprintf(stderr, "总容量按字节编址计算，并且要知道主存地址位数\n");
        return 1;
    }
    out->tag_bits = f->tag_bits;
    out->valid_bits = 1;
    out->dirty_bits = write_back ? 1 : 0;
    /* 直接映射的那一行是唯一去处，计数器没有信息量。 */
    out->replace_bits = (replace != REPLACE_NONE && f->n_ways > 1) ? log2_pow2(f->n_ways) : 0;
    meta = out->tag_bits + out->valid_bits + out->dirty_bits + out->replace_bits;
    out->meta_bits = (long long)f->n_lines * meta;
    out->data_bits = (long long)f->n_lines * f->block_units * 8;
    out->total_bits = out->meta_bits + out->data_bits;
    return 0;
}

static void show_map_kind(FILE *out, const Fields *f) {
    fprintf(out, "组数 = %d / %d = %d\n", f->n_lines, f->n_ways, f->n_sets);
    if (f->n_ways == 1) {
        fprintf(out, "直接映射时组号就是行号，主存块只能进这一行\n");
    } else if (f->n_sets == 1) {
        fprintf(out, "全相联没有组号，主存块可以装进任意一行\n");
    } else {
        fprintf(out, "组间直接映射，组内 %d 路全相联\n", f->n_ways);
    }
}

int show_fields(FILE *out, const Fields *f) {
    const char *unit = f->word ? "字" : "B";
    if (f->tag_bits < 0) {
        fprintf(stderr, "划分地址字段需要主存地址位数\n");
        return 1;
    }
    fprintf(out, "【地址划分】\n");
    fprintf(out, "按%s编址，主存地址 %d 位\n", f->word ? "字" : "字节", f->addr_bits);
    fprintf(out, "Cache %d 行，每组 %d 路，%s，块 %d %s\n", f->n_lines, f->n_ways, fields_map_name(f),
            f->block_units, unit);
    show_map_kind(out, f);
    fprintf(out, "块内偏移 = log2(%d) = %d 位\n", f->block_units, f->offset_bits);
    fprintf(out, "组号 = log2(%d) = %d 位\n", f->n_sets, f->index_bits);
    fprintf(out, "标记 = %d - %d - %d = %d 位\n", f->addr_bits, f->index_bits, f->offset_bits, f->tag_bits);
    fprintf(out, "主存地址从高到低：标记 %d 位 | 组号 %d 位 | 块内偏移 %d 位\n", f->tag_bits, f->index_bits,
            f->offset_bits);
    fprintf(out, "数据区 = %d × %d %s = %d %s\n", f->n_lines, f->block_units, unit,
            f->n_lines * f->block_units, unit);
    fprintf(out, "结果：标记 = %d 位，组号 = %d 位，块内偏移 = %d 位\n", f->tag_bits, f->index_bits,
            f->offset_bits);
    return 0;
}

int show_map(FILE *out, const Fields *f, uint64_t addr) {
    uint64_t block = 0;
    uint64_t tag = 0;
    uint64_t offset = 0;
    int set = 0;
    const char *unit = f->word ? "字" : "B";
    if (fields_locate(f, addr, &block, &set, &tag, &offset) != 0) {
        return 1;
    }
    fprintf(out, "【地址映射】\n");
    fprintf(out, "按%s编址，块 %d %s，%d 行，每组 %d 路，%s，组数 %d\n", f->word ? "字" : "字节",
            f->block_units, unit, f->n_lines, f->n_ways, fields_map_name(f), f->n_sets);
    fprintf(out, "地址 %llu\n", (unsigned long long)addr);
    fprintf(out, "块号 = %llu / %d = %llu\n", (unsigned long long)addr, f->block_units,
            (unsigned long long)block);
    fprintf(out, "组号 = %llu mod %d = %d\n", (unsigned long long)block, f->n_sets, set);
    fprintf(out, "块内偏移 = %llu mod %d = %llu\n", (unsigned long long)addr, f->block_units,
            (unsigned long long)offset);
    if (f->n_ways == 1) {
        fprintf(out, "结果：块号 = %llu，行号 = %d，块内偏移 = %llu\n", (unsigned long long)block, set,
                (unsigned long long)offset);
    } else {
        fprintf(out, "结果：块号 = %llu，组号 = %d，块内偏移 = %llu\n", (unsigned long long)block, set,
                (unsigned long long)offset);
    }
    return 0;
}

static void show_replace_bits(FILE *out, const Fields *f, int replace, int bits) {
    if (f->n_ways == 1) {
        fprintf(out, "直接映射只有 1 路，没有另一行可换，替换位 = 0\n");
        return;
    }
    if (replace == REPLACE_NONE || bits == 0) {
        fprintf(out, "本题不计替换位\n");
        return;
    }
    fprintf(out, "%s 计数器：每行 log2(%d) = %d 位\n", replace == REPLACE_LRU ? "LRU" : "FIFO", f->n_ways,
            bits);
}

int show_capacity(FILE *out, const Fields *f, int write_back, int replace) {
    Capacity cap;
    int meta;
    if (capacity_make(&cap, f, write_back, replace) != 0) {
        return 1;
    }
    meta = cap.tag_bits + cap.valid_bits + cap.dirty_bits + cap.replace_bits;
    fprintf(out, "【Cache 总容量】\n");
    fprintf(out, "按字节编址，主存地址 %d 位\n", f->addr_bits);
    fprintf(out, "%d 行 × %d B，每组 %d 路，%s\n", f->n_lines, f->block_units, f->n_ways,
            fields_map_name(f));
    fprintf(out, "标记 = %d 位。有效位表示这一行的数据是否有效，每行 %d 位\n", cap.tag_bits, cap.valid_bits);
    if (write_back) {
        fprintf(out, "写回：这一行比主存新时才在换出时写回，脏位 = %d\n", cap.dirty_bits);
    } else {
        fprintf(out, "写直达：写入时同时改主存，脏位 = %d\n", cap.dirty_bits);
    }
    show_replace_bits(out, f, replace, cap.replace_bits);
    fprintf(out, "每行标记阵列 = %d + %d + %d + %d = %d 位\n", cap.tag_bits, cap.valid_bits, cap.dirty_bits,
            cap.replace_bits, meta);
    fprintf(out, "数据区 = %d × %d B = %d B = %lld 位\n", f->n_lines, f->block_units,
            f->n_lines * f->block_units, cap.data_bits);
    fprintf(out, "标记阵列 = %d × %d 位 = %lld 位\n", f->n_lines, meta, cap.meta_bits);
    if (cap.total_bits % 1024 == 0) {
        fprintf(out, "%lld 位 = %lld × 2^10 位 = %lld K 位，存储容量里的 K 是 2^10\n", cap.total_bits,
                cap.total_bits / 1024, cap.total_bits / 1024);
    }
    if (cap.total_bits % 8 == 0) {
        fprintf(out, "结果：标记 = %d 位，有效位 = %d 位，脏位 = %d 位，替换位 = %d 位/行，总容量 = %lld 位 = %lld B\n",
                cap.tag_bits, cap.valid_bits, cap.dirty_bits, cap.replace_bits, cap.total_bits,
                cap.total_bits / 8);
    } else {
        fprintf(out, "结果：标记 = %d 位，有效位 = %d 位，脏位 = %d 位，替换位 = %d 位/行，总容量 = %lld 位\n",
                cap.tag_bits, cap.valid_bits, cap.dirty_bits, cap.replace_bits, cap.total_bits);
    }
    return 0;
}

int show_rate(FILE *out, int accesses, int misses) {
    char hit_pct[32];
    char miss_pct[32];
    int hits;
    if (accesses <= 0 || misses < 0 || misses > accesses) {
        fprintf(stderr, "访存次数要大于 0，缺失次数不能超过访存次数\n");
        return 1;
    }
    hits = accesses - misses;
    format_percent(hit_pct, sizeof hit_pct, hits, accesses);
    format_percent(miss_pct, sizeof miss_pct, misses, accesses);
    fprintf(out, "【命中率】\n");
    fprintf(out, "访存 %d 次，缺失 %d 次，命中 %d 次\n", accesses, misses, hits);
    fprintf(out, "命中率 = 命中次数 / 访存次数，缺失率 = 1 - 命中率\n");
    fprintf(out, "结果：命中率 = %s，缺失率 = %s\n", hit_pct, miss_pct);
    return 0;
}
