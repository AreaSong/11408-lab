#include "cache.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int valid;
    int dirty;
    uint64_t tag;
    uint64_t stamp;
} Line;

struct Cache {
    Line *lines;
    int addr_bits;
    int n_ways;
    int n_sets;
    int offset_bits;
    int index_bits;
    int replace;
    int write_back;
    int write_alloc;
    uint64_t clock;
    CacheStat stat;
};

static Line *line_at(Cache *c, int set, int way) {
    return &c->lines[set * c->n_ways + way];
}

int cache_open(Cache **out, const Fields *f, int replace, int write_back, int write_alloc) {
    Cache *c;
    if (f->tag_bits < 0 || (replace != REPLACE_LRU && replace != REPLACE_FIFO)) {
        fprintf(stderr, "模拟访问需要主存地址位数，替换用 lru 或 fifo\n");
        return 1;
    }
    c = calloc(1, sizeof(*c));
    if (c == NULL) {
        return 1;
    }
    c->lines = calloc((size_t)f->n_lines, sizeof(Line));
    if (c->lines == NULL) {
        free(c);
        return 1;
    }
    c->addr_bits = f->addr_bits;
    c->n_ways = f->n_ways;
    c->n_sets = f->n_sets;
    c->offset_bits = f->offset_bits;
    c->index_bits = f->index_bits;
    c->replace = replace;
    c->write_back = write_back ? 1 : 0;
    c->write_alloc = write_alloc ? 1 : 0;
    *out = c;
    return 0;
}

void cache_close(Cache *c) {
    if (c == NULL) {
        return;
    }
    free(c->lines);
    free(c);
}

void cache_stat(const Cache *c, CacheStat *out) {
    *out = c->stat;
}

int cache_ways(const Cache *c) {
    return c->n_ways;
}

int cache_peek(const Cache *c, int set, int way, int *valid, int *dirty, uint64_t *block) {
    const Line *ln;
    if (set < 0 || set >= c->n_sets || way < 0 || way >= c->n_ways) {
        return 1;
    }
    ln = &c->lines[set * c->n_ways + way];
    *valid = ln->valid;
    *dirty = ln->dirty;
    /* 标记左移组号的位数，再拼上组号，就是主存块号。 */
    *block = ln->valid ? ((ln->tag << c->index_bits) | (uint64_t)set) : 0;
    return 0;
}

static int find_way(const Cache *c, int set, uint64_t tag) {
    int way;
    for (way = 0; way < c->n_ways; way++) {
        const Line *ln = &c->lines[set * c->n_ways + way];
        if (ln->valid && ln->tag == tag) {
            return way;
        }
    }
    return -1;
}

static void touch(Cache *c, int set, int way) {
    line_at(c, set, way)->stamp = ++c->clock;
}

/* 空行优先，否则选 stamp 最小的。stamp 只在装入时增加就是 FIFO，命中也增加就是 LRU。 */
static int victim_way(Cache *c, int set, int *was_valid, int *dirty, uint64_t *old_block) {
    int way;
    int oldest = 0;
    *was_valid = 0;
    *dirty = 0;
    *old_block = 0;
    for (way = 0; way < c->n_ways; way++) {
        if (!line_at(c, set, way)->valid) {
            return way;
        }
    }
    for (way = 1; way < c->n_ways; way++) {
        if (line_at(c, set, way)->stamp < line_at(c, set, oldest)->stamp) {
            oldest = way;
        }
    }
    *was_valid = 1;
    *dirty = line_at(c, set, oldest)->dirty;
    *old_block = (line_at(c, set, oldest)->tag << c->index_bits) | (uint64_t)set;
    return oldest;
}

static void install(Cache *c, int set, int way, uint64_t tag) {
    Line *ln = line_at(c, set, way);
    ln->valid = 1;
    ln->dirty = 0;
    ln->tag = tag;
    ln->stamp = ++c->clock;
}

static void note_write(Cache *c, int set, int way, AccessInfo *info) {
    if (c->write_back) {
        line_at(c, set, way)->dirty = 1;
        return;
    }
    c->stat.mem_writes++;
    info->mem_write = 1;
}

static void split_addr(const Cache *c, uint64_t addr, AccessInfo *info) {
    if (c->offset_bits == 0) {
        info->block = addr;
        info->offset = 0;
    } else {
        info->block = addr >> c->offset_bits;
        info->offset = addr & ((1ull << c->offset_bits) - 1);
    }
    info->set = (int)(info->block & (uint64_t)(c->n_sets - 1));
    info->tag = info->block >> c->index_bits;
}

static void fill_miss(Cache *c, int is_write, AccessInfo *info) {
    int was_valid = 0;
    int dirty = 0;
    uint64_t old_block = 0;
    int way = victim_way(c, info->set, &was_valid, &dirty, &old_block);
    info->way = way;
    info->replaced = was_valid;
    if (was_valid) {
        info->old_block = old_block;
    }
    if (was_valid && dirty && c->write_back) {
        c->stat.writebacks++;
        info->writeback = 1;
    }
    install(c, info->set, way, info->tag);
    c->stat.fills++;
    if (is_write) {
        note_write(c, info->set, way, info);
    }
}

int cache_access(Cache *c, int is_write, uint64_t addr, AccessInfo *info) {
    int way;
    memset(info, 0, sizeof(*info));
    info->way = -1;
    if (c->addr_bits > 0 && addr >= (1ull << c->addr_bits)) {
        fprintf(stderr, "地址 %llu 超出 %d 位主存地址\n", (unsigned long long)addr, c->addr_bits);
        return 1;
    }
    split_addr(c, addr, info);
    way = find_way(c, info->set, info->tag);
    if (way >= 0) {
        c->stat.hits++;
        info->hit = 1;
        info->way = way;
        if (c->replace == REPLACE_LRU) {
            touch(c, info->set, way);
        }
        if (is_write) {
            note_write(c, info->set, way, info);
        }
        return 0;
    }
    c->stat.misses++;
    if (is_write && !c->write_alloc) {
        c->stat.mem_writes++;
        info->mem_write = 1;
        info->bypass = 1;
        return 0;
    }
    fill_miss(c, is_write, info);
    return 0;
}
