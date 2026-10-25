#ifndef FIELDS_H
#define FIELDS_H

#include <stdint.h>
#include <stdio.h>

/* 替换位按计数器法：路数大于 1 时每行 log2(路数) 位。直接映射没有选择，是 0。 */
enum {
    REPLACE_NONE = 0,
    REPLACE_LRU = 1,
    REPLACE_FIFO = 2
};

typedef struct {
    int word; /* 1 按字编址，0 按字节编址 */
    int addr_bits; /* 0 表示题目没给主存地址位数，标记位数未知 */
    int block_units;
    int n_lines;
    int n_ways;
    int n_sets;
    int offset_bits;
    int index_bits;
    int tag_bits; /* 地址位数未知时为 -1 */
} Fields;

typedef struct {
    int tag_bits;
    int valid_bits;
    int dirty_bits;
    int replace_bits;
    long long meta_bits;
    long long data_bits;
    long long total_bits;
} Capacity;

/* 行数、路数、块大小都要是 2 的幂，路数整除行数。失败时已向 stderr 说明。 */
int fields_make(Fields *out, int word, int addr_bits, int block_units, int n_lines, int n_ways);

int fields_locate(const Fields *f, uint64_t addr, uint64_t *block, int *set, uint64_t *tag,
                  uint64_t *offset);

/* 数据按字节计。写回才有脏位。替换位见 REPLACE_* 。 */
int capacity_make(Capacity *out, const Fields *f, int write_back, int replace);

const char *fields_map_name(const Fields *f);

void format_percent(char *dst, size_t n, int part, int total);

int show_fields(FILE *out, const Fields *f);
int show_map(FILE *out, const Fields *f, uint64_t addr);
int show_capacity(FILE *out, const Fields *f, int write_back, int replace);
int show_rate(FILE *out, int accesses, int misses);

#endif
