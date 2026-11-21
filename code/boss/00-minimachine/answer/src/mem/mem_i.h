#ifndef MEM_I_H
#define MEM_I_H

#include "mem/mem_cache.h"

#include <stdint.h>

typedef struct {
    int valid;
    int dirty;
    uint32_t tag;
    uint32_t stamp;
} MemLine;

typedef struct {
    MemLine *lines;
    uint8_t *bytes;
    int addr_bits;
    int ways;
    int sets;
    int block;
    int lines_n;
    int offset_bits;
    int index_bits;
    int replace;
    int write_back;
    int write_alloc;
    int mem_cycles;
    uint32_t seed;
    uint32_t clock;
    int accesses;
    int hits;
    int misses;
    int fills;
    int writebacks;
    int mem_writes;
    int stall;
} MemStore;

typedef struct MemCache MemCache;

struct MemCache {
    MemCacheCfg cfg;
    MemStore inst;
    MemStore data;
};

int mem_store_open(MemStore *s, int addr_bits, const MemCacheCfg *cfg);
void mem_store_close(MemStore *s);
void mem_store_cold(MemStore *s);
int mem_store_access(MemStore *s, MmMachine *m, int is_write, uint32_t addr, int size, uint32_t *data,
                     MemAccess *info);

int mem_pow2_bits(uint32_t n);
void mem_format_percent(char *dst, size_t n, int part, int total);

#endif
