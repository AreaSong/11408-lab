#include "mem/mem_i.h"

#include "common/mm_internal.h"

#include <stdlib.h>
#include <string.h>

static MemLine *line_at(MemStore *s, int set, int way) {
    return &s->lines[set * s->ways + way];
}

static int find_way(const MemStore *s, int set, uint32_t tag) {
    int way;
    for (way = 0; way < s->ways; way++) {
        const MemLine *ln = &s->lines[set * s->ways + way];
        if (ln->valid && ln->tag == tag) {
            return way;
        }
    }
    return -1;
}

static void touch_line(MemStore *s, MemLine *ln, int install) {
    if (s->replace == MEM_REPL_RAND) {
        return;
    }
    if (s->replace == MEM_REPL_FIFO && !install) {
        return;
    }
    ln->stamp = ++s->clock;
}

static int pick_victim(MemStore *s, int set) {
    int way;
    int best = 0;
    for (way = 0; way < s->ways; way++) {
        if (!line_at(s, set, way)->valid) {
            return way;
        }
    }
    if (s->replace == MEM_REPL_RAND) {
        s->seed = s->seed * 1103515245u + 12345u;
        return (int)((s->seed >> 16) % (uint32_t)s->ways);
    }
    for (way = 1; way < s->ways; way++) {
        if (line_at(s, set, way)->stamp < line_at(s, set, best)->stamp) {
            best = way;
        }
    }
    return best;
}

static uint32_t load_bytes(const MemStore *s, int index, int off, int size) {
    const uint8_t *p = s->bytes + index * s->block + off;
    uint32_t value = 0;
    int i;
    for (i = 0; i < size; i++) {
        value |= (uint32_t)p[i] << (8 * i);
    }
    return value;
}

static int in_range(const MemStore *s, MmMachine *m, uint32_t addr, int size) {
    uint32_t base = addr & ~((uint32_t)s->block - 1u);
    if (s->addr_bits < 32 && (s->addr_bits <= 0 || addr >= (1u << s->addr_bits))) {
        return 0;
    }
    if ((addr & (uint32_t)(s->block - 1)) + (uint32_t)size > (uint32_t)s->block) {
        return 0;
    }
    if (m == NULL) {
        return 1;
    }
    if (addr > m->mem_size || (uint32_t)size > m->mem_size - addr) {
        return 0;
    }
    if (base > m->mem_size || (uint32_t)s->block > m->mem_size - base) {
        return 0;
    }
    return 1;
}

static int fill_line(MemStore *s, MmMachine *m, int index, uint32_t addr) {
    uint32_t base = addr & ~((uint32_t)s->block - 1u);
    uint8_t *dst = s->bytes + index * s->block;
    int i;
    if (m == NULL) {
        memset(dst, 0, (size_t)s->block);
        return 0;
    }
    for (i = 0; i < s->block; i++) {
        dst[i] = m->mem[base + (uint32_t)i];
    }
    return 0;
}

static int push_back(MemStore *s, MmMachine *m, const MemLine *ln, int set, int index) {
    uint32_t base = ((ln->tag << s->index_bits) | (uint32_t)set) << s->offset_bits;
    uint8_t *src = s->bytes + index * s->block;
    int i;
    if (m == NULL) {
        return 0;
    }
    if (base > m->mem_size || (uint32_t)s->block > m->mem_size - base) {
        return -1;
    }
    for (i = 0; i < s->block; i++) {
        m->mem[base + (uint32_t)i] = src[i];
    }
    return 0;
}

static void note_write(MemStore *s, MmMachine *m, MemLine *ln, int index, int off, int size, uint32_t addr,
                       uint32_t data) {
    uint8_t *p = s->bytes + index * s->block + off;
    int i;
    for (i = 0; i < size; i++) {
        p[i] = (uint8_t)((data >> (8 * i)) & 0xffu);
    }
    if (s->write_back) {
        ln->dirty = 1;
        return;
    }
    /* 写直达当时改主存。命中的那一拍已经算过，这里不再加拍。 */
    s->mem_writes++;
    if (m == NULL) {
        return;
    }
    for (i = 0; i < size; i++) {
        m->mem[addr + (uint32_t)i] = p[i];
    }
}

static void bypass_write(MemStore *s, MmMachine *m, uint32_t addr, int size, uint32_t data) {
    int i;
    s->mem_writes++;
    if (m == NULL) {
        return;
    }
    for (i = 0; i < size; i++) {
        m->mem[addr + (uint32_t)i] = (uint8_t)((data >> (8 * i)) & 0xffu);
    }
}

static int miss_install(MemStore *s, MmMachine *m, int set, uint32_t tag, uint32_t addr, int *way, int *wrote) {
    int choice = pick_victim(s, set);
    int index = set * s->ways + choice;
    MemLine *ln = line_at(s, set, choice);
    *way = choice;
    *wrote = 0;
    if (ln->valid && ln->dirty && s->write_back) {
        if (push_back(s, m, ln, set, index) != 0) {
            return -1;
        }
        ln->dirty = 0;
        s->writebacks++;
        *wrote = 1;
    }
    if (fill_line(s, m, index, addr) != 0) {
        return -1;
    }
    s->fills++;
    ln->valid = 1;
    ln->tag = tag;
    ln->dirty = 0;
    touch_line(s, ln, 1);
    return 0;
}

static int miss_stall(const MemStore *s, int wrote) {
    int fill = s->mem_cycles * (s->block / 4);
    return wrote ? fill * 2 : fill;
}

int mem_store_open(MemStore *s, int addr_bits, const MemCacheCfg *cfg) {
    int offset = mem_pow2_bits((uint32_t)cfg->block_bytes);
    int sets = cfg->lines / cfg->ways;
    int index = mem_pow2_bits((uint32_t)sets);
    memset(s, 0, sizeof *s);
    if (offset < 0 || index < 0 || addr_bits < offset + index || addr_bits > 32) {
        return -1;
    }
    s->lines = calloc((size_t)cfg->lines, sizeof(MemLine));
    s->bytes = calloc((size_t)cfg->lines * (size_t)cfg->block_bytes, 1);
    if (s->lines == NULL || s->bytes == NULL) {
        mem_store_close(s);
        return -1;
    }
    s->addr_bits = addr_bits;
    s->ways = cfg->ways;
    s->sets = sets;
    s->block = cfg->block_bytes;
    s->lines_n = cfg->lines;
    s->offset_bits = offset;
    s->index_bits = index;
    s->replace = cfg->replace;
    s->write_back = cfg->write_back ? 1 : 0;
    s->write_alloc = cfg->write_alloc ? 1 : 0;
    s->mem_cycles = cfg->mem_cycles;
    s->seed = cfg->seed;
    return 0;
}

void mem_store_close(MemStore *s) {
    if (s == NULL) {
        return;
    }
    free(s->lines);
    free(s->bytes);
    s->lines = NULL;
    s->bytes = NULL;
}

void mem_store_cold(MemStore *s) {
    memset(s->lines, 0, (size_t)s->lines_n * sizeof(MemLine));
    memset(s->bytes, 0, (size_t)s->lines_n * (size_t)s->block);
    s->clock = 0;
    s->accesses = 0;
    s->hits = 0;
    s->misses = 0;
    s->fills = 0;
    s->writebacks = 0;
    s->mem_writes = 0;
    s->stall = 0;
}

static void use_line(MemStore *s, MmMachine *m, int set, int way, int is_write, uint32_t addr, int off, int size,
                     uint32_t *data, int refresh) {
    MemLine *ln = line_at(s, set, way);
    int index = set * s->ways + way;
    if (refresh) {
        touch_line(s, ln, 0);
    }
    if (is_write) {
        note_write(s, m, ln, index, off, size, addr, *data);
    } else if (data != NULL) {
        *data = load_bytes(s, index, off, size);
    }
}

int mem_store_access(MemStore *s, MmMachine *m, int is_write, uint32_t addr, int size, uint32_t *data,
                     MemAccess *info) {
    MemAccess local;
    uint32_t block;
    uint32_t tag;
    int set;
    int way;
    int off;
    int wrote = 0;
    if (info == NULL) {
        info = &local;
    }
    memset(info, 0, sizeof *info);
    info->way = -1;
    if ((size != 1 && size != 4) || (is_write && data == NULL) || !in_range(s, m, addr, size)) {
        return -1;
    }
    block = s->offset_bits == 0 ? addr : addr >> s->offset_bits;
    set = (int)(block & (uint32_t)(s->sets - 1));
    tag = s->index_bits == 0 ? block : block >> s->index_bits;
    off = (int)(addr & (uint32_t)(s->block - 1));
    s->accesses++;
    info->set = set;
    info->tag = tag;
    info->block = block;
    way = find_way(s, set, tag);
    if (way >= 0) {
        s->hits++;
        info->hit = 1;
        info->way = way;
        use_line(s, m, set, way, is_write, addr, off, size, data, 1);
        return 0;
    }
    s->misses++;
    if (is_write && !s->write_alloc) {
        info->bypass = 1;
        info->stall = s->mem_cycles;
        s->stall += info->stall;
        bypass_write(s, m, addr, size, *data);
        return 0;
    }
    if (miss_install(s, m, set, tag, addr, &way, &wrote) != 0) {
        return -1;
    }
    info->way = way;
    info->writeback = wrote;
    info->stall = miss_stall(s, wrote);
    s->stall += info->stall;
    /* 装入时已经按 FIFO/LRU 记过次序，这里不要再刷新。 */
    use_line(s, m, set, way, is_write, addr, off, size, data, 0);
    return 0;
}
