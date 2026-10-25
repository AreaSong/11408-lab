#ifndef CACHE_H
#define CACHE_H

#include "fields.h"

#include <stdint.h>

typedef struct Cache Cache;

typedef struct {
    int hits;
    int misses;
    int fills;
    int writebacks;
    int mem_writes;
} CacheStat;

typedef struct {
    int hit;
    int set;
    int way; /* 未调块时为 -1 */
    uint64_t block;
    uint64_t tag;
    uint64_t offset;
    int bypass; /* 写缺失且非写分配：只写主存 */
    int replaced;
    int writeback;
    uint64_t old_block;
    int mem_write;
} AccessInfo;

int cache_open(Cache **out, const Fields *f, int replace, int write_back, int write_alloc);
void cache_close(Cache *c);

/* 读 is_write = 0。地址超出给定的主存位数时返回 1。 */
int cache_access(Cache *c, int is_write, uint64_t addr, AccessInfo *info);

void cache_stat(const Cache *c, CacheStat *out);
int cache_ways(const Cache *c);
int cache_peek(const Cache *c, int set, int way, int *valid, int *dirty, uint64_t *block);

#endif
