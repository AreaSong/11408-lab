#include "mem/mem_i.h"

#include <stdio.h>
#include <string.h>

int mem_pow2_bits(uint32_t n) {
    int bits = 0;
    uint32_t value = 1;
    if (n == 0) {
        return -1;
    }
    while (value < n) {
        if (bits >= 31) {
            return -1;
        }
        value <<= 1;
        bits++;
    }
    if (value != n) {
        return -1;
    }
    return bits;
}

void mem_format_percent(char *dst, size_t n, int part, int total) {
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
    if ((long long)part * 10000 % total != 0) {
        snprintf(dst, n, "%d/%d", part, total);
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

static int geom_bits(int lines, int ways, int block, int *sets, int *offset, int *index) {
    int set_n;
    if (mem_pow2_bits((uint32_t)block) < 0 || mem_pow2_bits((uint32_t)lines) < 0 ||
        mem_pow2_bits((uint32_t)ways) < 0 || ways > lines || lines % ways != 0) {
        return -1;
    }
    set_n = lines / ways;
    *sets = set_n;
    *offset = mem_pow2_bits((uint32_t)block);
    *index = mem_pow2_bits((uint32_t)set_n);
    return (*offset < 0 || *index < 0) ? -1 : 0;
}

int mem_cache_geom(int addr_bits, int lines, int ways, int block, int write_back, int replace, MemGeom *out) {
    int sets = 0;
    int offset = 0;
    int index = 0;
    int meta;
    if (out == NULL || geom_bits(lines, ways, block, &sets, &offset, &index) != 0) {
        return -1;
    }
    if (addr_bits < offset + index || addr_bits > 32) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    out->lines = lines;
    out->ways = ways;
    out->sets = sets;
    out->offset_bits = offset;
    out->index_bits = index;
    out->tag_bits = addr_bits - offset - index;
    out->valid_bits = 1;
    out->dirty_bits = write_back ? 1 : 0;
    /* 直接映射没有选择。随机替换不设每行计数器。LRU/FIFO 是 log2(路数)。 */
    if ((replace == MEM_REPL_LRU || replace == MEM_REPL_FIFO) && ways > 1) {
        out->replace_bits = mem_pow2_bits((uint32_t)ways);
    }
    meta = out->tag_bits + out->valid_bits + out->dirty_bits + out->replace_bits;
    out->total_bits = (long long)lines * meta + (long long)lines * block * 8;
    return 0;
}

int mem_cache_map(int lines, int ways, int block, uint32_t addr, uint32_t *block_no, int *set, uint32_t *tag,
                  uint32_t *offset) {
    int sets = 0;
    int off_bits = 0;
    int index_bits = 0;
    uint32_t block_id;
    if (geom_bits(lines, ways, block, &sets, &off_bits, &index_bits) != 0 || block_no == NULL || set == NULL ||
        tag == NULL || offset == NULL) {
        return -1;
    }
    block_id = off_bits == 0 ? addr : addr >> off_bits;
    *block_no = block_id;
    *set = (int)(block_id & (uint32_t)(sets - 1));
    *tag = index_bits == 0 ? block_id : block_id >> index_bits;
    *offset = off_bits == 0 ? 0 : addr & ((1u << off_bits) - 1u);
    return 0;
}

static const char *map_name(const MemGeom *g) {
    if (g->ways <= 1) {
        return "直接映射";
    }
    if (g->sets <= 1) {
        return "全相联";
    }
    return "组相联";
}

void mem_geom_report(FILE *fp, const MemGeom *g, int addr_bits, int block) {
    fprintf(fp, "【地址划分】按字节编址，主存地址 %d 位\n", addr_bits);
    fprintf(fp, "%d 行，每组 %d 路，%s，块 %d B\n", g->lines, g->ways, map_name(g), block);
    fprintf(fp, "组数 = %d / %d = %d\n", g->lines, g->ways, g->sets);
    fprintf(fp, "块内偏移 = log2(%d) = %d 位\n", block, g->offset_bits);
    fprintf(fp, "组号 = log2(%d) = %d 位\n", g->sets, g->index_bits);
    fprintf(fp, "标记 = %d - %d - %d = %d 位\n", addr_bits, g->index_bits, g->offset_bits, g->tag_bits);
    fprintf(fp, "结果：标记 = %d 位，组号 = %d 位，块内偏移 = %d 位\n", g->tag_bits, g->index_bits,
            g->offset_bits);
}

void mem_cap_report(FILE *fp, const MemGeom *g) {
    int meta = g->tag_bits + g->valid_bits + g->dirty_bits + g->replace_bits;
    fprintf(fp, "【Cache 总容量】%d 行，每组 %d 路，%s\n", g->lines, g->ways, map_name(g));
    fprintf(fp, "标记 %d 位，有效位 %d 位，脏位 %d 位，替换位 %d 位\n", g->tag_bits, g->valid_bits,
            g->dirty_bits, g->replace_bits);
    fprintf(fp, "每行标记阵列 = %d 位。存储容量的 K 是 2^10\n", meta);
    if (g->total_bits % 1024 == 0) {
        fprintf(fp, "结果：总容量 = %lld 位 = %lld K 位\n", g->total_bits, g->total_bits / 1024);
        return;
    }
    if (g->total_bits % 8 == 0) {
        fprintf(fp, "结果：总容量 = %lld 位 = %lld B\n", g->total_bits, g->total_bits / 8);
        return;
    }
    fprintf(fp, "结果：总容量 = %lld 位\n", g->total_bits);
}

void mem_map_report(FILE *fp, int lines, int ways, int block, uint32_t addr) {
    uint32_t block_no = 0;
    uint32_t tag = 0;
    uint32_t offset = 0;
    int set = 0;
    if (mem_cache_map(lines, ways, block, addr, &block_no, &set, &tag, &offset) != 0) {
        fprintf(fp, "错误：映射参数不是 2 的幂\n");
        return;
    }
    fprintf(fp, "【地址映射】地址 %u，块 %d B，%d 行，每组 %d 路\n", addr, block, lines, ways);
    fprintf(fp, "块号 = %u / %d = %u，组号 = %u mod %d = %d，块内偏移 = %u\n", addr, block, block_no, block_no,
            lines / ways, set, offset);
    if (ways == 1) {
        fprintf(fp, "结果：块号 = %u，行号 = %d，块内偏移 = %u\n", block_no, set, offset);
    } else {
        fprintf(fp, "结果：块号 = %u，组号 = %d，块内偏移 = %u\n", block_no, set, offset);
    }
}
