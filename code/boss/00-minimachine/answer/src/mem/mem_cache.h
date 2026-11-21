#ifndef MEM_CACHE_H
#define MEM_CACHE_H

#include "common/mm_machine.h"

#include <stdint.h>
#include <stdio.h>

/* 替换：LRU 命中也刷新，FIFO 只在装入时排队，随机用固定种子。 */
enum { MEM_REPL_LRU = 1, MEM_REPL_FIFO = 2, MEM_REPL_RAND = 3 };

typedef struct {
    int on;
    int lines;
    int ways;
    int block_bytes;
    int replace;
    int write_back;
    int write_alloc;
    int mem_cycles; /* 主存传一个字要几个 CPU 周期 */
    int split;      /* 1：指令 Cache 和数据 Cache 各一份 */
    uint32_t seed;
} MemCacheCfg;

typedef struct {
    int lines;
    int ways;
    int sets;
    int offset_bits;
    int index_bits;
    int tag_bits;
    int valid_bits;
    int dirty_bits;
    int replace_bits;
    long long total_bits;
} MemGeom;

typedef struct {
    int hit;
    int set;
    int way;
    uint32_t tag;
    uint32_t block;
    int writeback;
    int bypass;
    int stall; /* 这一次在「查 Cache 的 1 拍」之外还要等的拍数 */
} MemAccess;

typedef struct {
    int accesses;
    int hits;
    int misses;
    int fills;
    int writebacks;
    int mem_writes;
    int stall;
} MemCacheStat;

typedef struct {
    int accesses;
    int hits;
    int misses;
} MemWalkOut;

void mem_cache_cfg_default(MemCacheCfg *cfg);
int mem_cache_cfg_ok(const MemCacheCfg *cfg);
int mem_cache_set(MmMachine *m, const MemCacheCfg *cfg);
int mem_cache_on(const MmMachine *m);
void mem_cache_drop(MmMachine *m);

/* 地址字段和总容量。replace 用 MEM_REPL_*。直接映射和随机替换的替换位是 0。 */
int mem_cache_geom(int addr_bits, int lines, int ways, int block, int write_back, int replace,
                   MemGeom *out);
int mem_cache_map(int lines, int ways, int block, uint32_t addr, uint32_t *block_no, int *set,
                  uint32_t *tag, uint32_t *offset);

/* which：0 数据，1 指令，2 两块相加。统一 Cache 时指令侧是 0，访问都记在数据侧。 */
int mem_cache_access(MmMachine *m, int is_inst, int is_write, uint32_t addr, int size, uint32_t *data,
                     MemAccess *info);
int mem_cache_peek(const MmMachine *m, int is_inst, int set, int way, int *valid, int *dirty,
                   uint32_t *block_no);
void mem_cache_stat(const MmMachine *m, int which, MemCacheStat *out);

/* 元素 4 字节，按行优先存放。按行、按列各从空 Cache 开始，只读。 */
int mem_cache_walk(int addr_bits, int lines, int ways, int block, uint32_t base, int rows, int cols,
                   MemWalkOut *row_major, MemWalkOut *col_major);

/* 本拍指令缺失和数据缺失：分开时取较大值，共用一块时相加。返回后清掉。 */
int mem_cache_take_stall(MmMachine *m);

int mem_cpu_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mem_cpu_write(MmMachine *m, uint32_t addr, int size, uint32_t data);

void mem_geom_report(FILE *fp, const MemGeom *g, int addr_bits, int block);
void mem_cap_report(FILE *fp, const MemGeom *g);
void mem_map_report(FILE *fp, int lines, int ways, int block, uint32_t addr);
void mem_walk_report(FILE *fp, int addr_bits, int lines, int ways, int block, uint32_t base, int rows,
                     int cols);
void mem_cache_report(const MmMachine *m, FILE *fp);

#endif
