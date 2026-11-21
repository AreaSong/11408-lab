#include "mem/mem_i.h"

#include "common/mm_internal.h"

#include <stdlib.h>
#include <string.h>

void mem_cache_cfg_default(MemCacheCfg *cfg) {
    memset(cfg, 0, sizeof *cfg);
    cfg->lines = 8;
    cfg->ways = 1;
    cfg->block_bytes = 16;
    cfg->replace = MEM_REPL_LRU;
    cfg->write_back = 1;
    cfg->write_alloc = 1;
    cfg->mem_cycles = 4;
    cfg->split = 1;
    cfg->seed = 1;
}

int mem_cache_cfg_ok(const MemCacheCfg *cfg) {
    if (cfg == NULL) {
        return 0;
    }
    if (!cfg->on) {
        return 1;
    }
    if (mem_pow2_bits((uint32_t)cfg->lines) < 0 || cfg->lines > 4096) {
        return 0;
    }
    if (mem_pow2_bits((uint32_t)cfg->ways) < 0 || cfg->ways > cfg->lines || cfg->lines % cfg->ways != 0) {
        return 0;
    }
    if (mem_pow2_bits((uint32_t)cfg->block_bytes) < 0 || cfg->block_bytes < 4 || cfg->block_bytes > 256) {
        return 0;
    }
    if (cfg->mem_cycles < 1 || cfg->mem_cycles > 100000) {
        return 0;
    }
    if (cfg->replace < MEM_REPL_LRU || cfg->replace > MEM_REPL_RAND) {
        return 0;
    }
    return 1;
}

void mem_cache_drop(MmMachine *m) {
    if (m == NULL) {
        return;
    }
    if (m->cache != NULL) {
        mem_store_close(&m->cache->inst);
        mem_store_close(&m->cache->data);
        free(m->cache);
        m->cache = NULL;
    }
    m->mem_if = 0;
    m->pend_i = 0;
    m->pend_d = 0;
    m->mem_stall = 0;
}

int mem_cache_on(const MmMachine *m) { return m != NULL && m->cache != NULL; }

int mem_cache_set(MmMachine *m, const MemCacheCfg *cfg) {
    int bits;
    MemCache *cache;
    mem_cache_drop(m);
    if (m == NULL || cfg == NULL || !cfg->on) {
        return cfg != NULL && !cfg->on ? 0 : -1;
    }
    if (!mem_cache_cfg_ok(cfg)) {
        return -1;
    }
    bits = mem_pow2_bits(m->mem_size);
    cache = calloc(1, sizeof *cache);
    if (bits < 0 || cache == NULL) {
        free(cache);
        return -1;
    }
    cache->cfg = *cfg;
    if (mem_store_open(&cache->data, bits, cfg) != 0) {
        free(cache);
        return -1;
    }
    if (cfg->split && mem_store_open(&cache->inst, bits, cfg) != 0) {
        mem_store_close(&cache->data);
        free(cache);
        return -1;
    }
    m->cache = cache;
    return 0;
}

static MemStore *pick_store(MmMachine *m, int is_inst) {
    if (is_inst && m->cache->cfg.split && m->cache->inst.lines != NULL) {
        return &m->cache->inst;
    }
    return &m->cache->data;
}

static void add_stat(MemCacheStat *out, const MemStore *s) {
    if (s->lines == NULL) {
        return;
    }
    out->accesses += s->accesses;
    out->hits += s->hits;
    out->misses += s->misses;
    out->fills += s->fills;
    out->writebacks += s->writebacks;
    out->mem_writes += s->mem_writes;
    out->stall += s->stall;
}

void mem_cache_stat(const MmMachine *m, int which, MemCacheStat *out) {
    memset(out, 0, sizeof *out);
    if (m->cache == NULL) {
        return;
    }
    if (which == 0 || which == 2) {
        add_stat(out, &m->cache->data);
    }
    if (which == 1 || which == 2) {
        add_stat(out, &m->cache->inst);
    }
}

int mem_cache_access(MmMachine *m, int is_inst, int is_write, uint32_t addr, int size, uint32_t *data,
                     MemAccess *info) {
    if (!mem_cache_on(m)) {
        return -1;
    }
    return mem_store_access(pick_store(m, is_inst), m, is_write, addr, size, data, info);
}

int mem_cache_peek(const MmMachine *m, int is_inst, int set, int way, int *valid, int *dirty, uint32_t *block_no) {
    const MemStore *s;
    const MemLine *ln;
    if (!mem_cache_on(m)) {
        return -1;
    }
    s = pick_store((MmMachine *)m, is_inst);
    if (set < 0 || way < 0 || set >= s->sets || way >= s->ways) {
        return -1;
    }
    ln = &s->lines[set * s->ways + way];
    *valid = ln->valid;
    *dirty = ln->dirty;
    *block_no = ln->valid ? ((ln->tag << s->index_bits) | (uint32_t)set) : 0;
    return 0;
}

static int cpu_touch(MmMachine *m, int is_write, uint32_t addr, int size, uint32_t *data) {
    MemAccess info;
    if (mem_store_access(pick_store(m, m->mem_if), m, is_write, addr, size, data, &info) != 0) {
        m->badva = addr;
        return MM_EX_MCHK;
    }
    if (m->mem_if) {
        m->pend_i += info.stall;
    } else {
        m->pend_d += info.stall;
    }
    return 0;
}

int mem_cpu_read(MmMachine *m, uint32_t addr, int size, uint32_t *data) {
    return cpu_touch(m, 0, addr, size, data);
}

int mem_cpu_write(MmMachine *m, uint32_t addr, int size, uint32_t data) {
    return cpu_touch(m, 1, addr, size, &data);
}

int mem_cache_take_stall(MmMachine *m) {
    int n;
    if (m->cache == NULL) {
        m->pend_i = 0;
        m->pend_d = 0;
        return 0;
    }
    /* 两块 Cache 可以同时缺，流水线只等多的那一边。共用一块就得排队。 */
    if (m->cache->cfg.split) {
        n = m->pend_i > m->pend_d ? m->pend_i : m->pend_d;
    } else {
        n = m->pend_i + m->pend_d;
    }
    m->pend_i = 0;
    m->pend_d = 0;
    m->mem_stall += n;
    return n;
}

static int one_elem(MemStore *s, uint32_t base, int cols, int row, int col) {
    uint32_t index = (uint32_t)row * (uint32_t)cols + (uint32_t)col;
    return mem_store_access(s, NULL, 0, base + index * 4u, 4, NULL, NULL);
}

static int scan_row(MemStore *s, uint32_t base, int rows, int cols) {
    int row;
    int col;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            if (one_elem(s, base, cols, row, col) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int scan_col(MemStore *s, uint32_t base, int rows, int cols) {
    int row;
    int col;
    for (col = 0; col < cols; col++) {
        for (row = 0; row < rows; row++) {
            if (one_elem(s, base, cols, row, col) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static void save_walk(const MemStore *s, MemWalkOut *out) {
    out->accesses = s->accesses;
    out->hits = s->hits;
    out->misses = s->misses;
}

int mem_cache_walk(int addr_bits, int lines, int ways, int block, uint32_t base, int rows, int cols,
                   MemWalkOut *row_major, MemWalkOut *col_major) {
    MemCacheCfg cfg;
    MemStore store;
    if (rows < 1 || cols < 1 || (long long)rows * cols > 2000000 || row_major == NULL || col_major == NULL) {
        return -1;
    }
    mem_cache_cfg_default(&cfg);
    cfg.on = 1;
    cfg.lines = lines;
    cfg.ways = ways;
    cfg.block_bytes = block;
    cfg.write_back = 0;
    cfg.write_alloc = 1;
    cfg.replace = MEM_REPL_LRU;
    cfg.mem_cycles = 1;
    if (!mem_cache_cfg_ok(&cfg) || mem_store_open(&store, addr_bits, &cfg) != 0) {
        return -1;
    }
    if (scan_row(&store, base, rows, cols) != 0) {
        mem_store_close(&store);
        return -1;
    }
    save_walk(&store, row_major);
    mem_store_cold(&store);
    if (scan_col(&store, base, rows, cols) != 0) {
        mem_store_close(&store);
        return -1;
    }
    save_walk(&store, col_major);
    mem_store_close(&store);
    return 0;
}

static void print_amat(FILE *fp, int accesses, int stall) {
    int sum = accesses + stall;
    int a = sum;
    int b = accesses;
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    if (a < 0) {
        a = -a;
    }
    fprintf(fp, "平均访问时间 = (%d + %d) / %d = ", accesses, stall, accesses);
    if (sum % accesses == 0) {
        fprintf(fp, "%d 个时钟周期\n", sum / accesses);
    } else {
        fprintf(fp, "%d/%d 个时钟周期\n", sum / a, accesses / a);
    }
}

static void report_store(FILE *fp, const char *title, const MemStore *s) {
    char pct[32];
    if (s->lines == NULL) {
        return;
    }
    if (s->accesses <= 0) {
        fprintf(fp, "【%s】还没有访问\n", title);
        return;
    }
    mem_format_percent(pct, sizeof pct, s->hits, s->accesses);
    fprintf(fp, "【%s】访问 %d 次，命中 %d 次，缺失 %d 次，命中率 = %s\n", title, s->accesses, s->hits,
            s->misses, pct);
    print_amat(fp, s->accesses, s->stall);
    fprintf(fp, "调入 %d 块，写回 %d 次，当时写主存 %d 次，缺失额外 %d 拍\n", s->fills, s->writebacks,
            s->mem_writes, s->stall);
}

static const char *repl_name(int replace) {
    if (replace == MEM_REPL_FIFO) {
        return "FIFO";
    }
    if (replace == MEM_REPL_RAND) {
        return "随机";
    }
    return "LRU";
}

static const char *map_of(const MemCacheCfg *cfg) {
    if (cfg->ways <= 1) {
        return "直接映射";
    }
    if (cfg->lines == cfg->ways) {
        return "全相联";
    }
    return "组相联";
}

void mem_cache_report(const MmMachine *m, FILE *fp) {
    const MemCacheCfg *cfg;
    if (!mem_cache_on(m)) {
        return;
    }
    cfg = &m->cache->cfg;
    fprintf(fp, "【Cache】%s %s %s %s %s\n", cfg->split ? "指令和数据分开" : "统一", map_of(cfg),
            cfg->write_back ? "写回" : "写直达", cfg->write_alloc ? "写分配" : "非写分配", repl_name(cfg->replace));
    fprintf(fp, "行数 %d 路数 %d 块 %d B 主存周期 %d\n", cfg->lines, cfg->ways, cfg->block_bytes, cfg->mem_cycles);
    if (cfg->split) {
        report_store(fp, "指令 Cache", &m->cache->inst);
        report_store(fp, "数据 Cache", &m->cache->data);
    } else {
        report_store(fp, "统一 Cache", &m->cache->data);
    }
    fprintf(fp, "缺失阻塞 %d 拍\n", m->mem_stall);
}

static void print_walk_line(FILE *fp, const char *title, const MemWalkOut *w) {
    char pct[32];
    mem_format_percent(pct, sizeof pct, w->hits, w->accesses);
    fprintf(fp, "【%s】访问 %d 次，命中 %d 次，缺失 %d 次，命中率 = %s\n", title, w->accesses, w->hits, w->misses,
            pct);
}

void mem_walk_report(FILE *fp, int addr_bits, int lines, int ways, int block, uint32_t base, int rows, int cols) {
    MemWalkOut row;
    MemWalkOut col;
    char row_pct[32];
    char col_pct[32];
    if (mem_cache_walk(addr_bits, lines, ways, block, base, rows, cols, &row, &col) != 0) {
        fprintf(fp, "错误：数组或 Cache 参数不合法\n");
        return;
    }
    fprintf(fp, "【数组访问】%d 行 × %d 列，元素 4 B，首地址 %u，按行优先存放\n", rows, cols, base);
    fprintf(fp, "地址 %d 位，%d 行，每组 %d 路，块 %d B。按行、按列各从空 Cache 开始\n", addr_bits, lines, ways,
            block);
    print_walk_line(fp, "按行", &row);
    print_walk_line(fp, "按列", &col);
    mem_format_percent(row_pct, sizeof row_pct, row.hits, row.accesses);
    mem_format_percent(col_pct, sizeof col_pct, col.hits, col.accesses);
    fprintf(fp, "结果：按行 %s，按列 %s\n", row_pct, col_pct);
}
