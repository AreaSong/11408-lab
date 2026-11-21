#include "mmu/mmu_walk.h"

#include "common/mm_internal.h"
#include "mmu/mmu_i.h"
#include "mmu/mmu_tlb.h"

#include <stdlib.h>
#include <string.h>

void mmu_drop(MmMachine *m) {
    if (m == NULL || m->mmu == NULL) {
        return;
    }
    free(m->mmu->line);
    free(m->mmu);
    m->mmu = NULL;
    m->mmu_stall = 0;
}

int mmu_on(const MmMachine *m) { return m != NULL && m->mmu != NULL; }

static int zero_page(MmMachine *m, uint32_t addr) {
    uint32_t i;
    for (i = 0; i < MMU_PAGE_BYTES; i += 4u) {
        if (mm_raw_write(m, addr + i, 4, 0) != 0) {
            return -1;
        }
    }
    return 0;
}

static int cfg_ok(const MmMachine *m, const MmuCfg *cfg, int *sets, int *bits) {
    if (m == NULL || cfg == NULL || mmu_tlb_geom(cfg->entries, cfg->ways, sets, bits) != 0) {
        return 0;
    }
    if (cfg->replace < MMU_REPL_LRU || cfg->replace > MMU_REPL_RAND) {
        return 0;
    }
    if (cfg->walk_cycles < 1 || cfg->walk_cycles > 100000) {
        return 0;
    }
    if ((cfg->ptbr & 0xfffu) != 0 || cfg->ptbr > m->mem_size) {
        return 0;
    }
    return MMU_PAGE_BYTES <= m->mem_size - cfg->ptbr;
}

int mmu_enable(MmMachine *m, const MmuCfg *cfg) {
    int sets = 0;
    int bits = 0;
    Mmu *u;
    if (!cfg_ok(m, cfg, &sets, &bits)) {
        return -1;
    }
    mmu_drop(m);
    u = calloc(1, sizeof *u);
    if (u == NULL) {
        return -1;
    }
    u->line = calloc((size_t)cfg->entries, sizeof(MmuLine));
    if (u->line == NULL) {
        free(u);
        return -1;
    }
    u->entries = cfg->entries;
    u->ways = cfg->ways;
    u->sets = sets;
    u->index_bits = bits;
    u->replace = cfg->replace;
    u->walk_cycles = cfg->walk_cycles;
    u->ptbr = cfg->ptbr;
    u->next_pt = cfg->ptbr + MMU_PAGE_BYTES;
    u->seed = 1;
    m->mmu = u;
    m->sr[MM_SR_PTBR] = cfg->ptbr;
    if (zero_page(m, cfg->ptbr) != 0) {
        mmu_drop(m);
        return -1;
    }
    return 0;
}

static int read_u32(MmMachine *m, uint32_t addr, uint32_t *word) {
    return mm_raw_read(m, addr, 4, word);
}

/* 这一级页表不在时，从页目录后面顺次拿一页。页表页本身不给用户。 */
static int ensure_pt(MmMachine *m, Mmu *u, uint32_t dir, uint32_t *pt_base) {
    uint32_t addr = mmu_slot(u->ptbr, dir);
    uint32_t pde = 0;
    if (read_u32(m, addr, &pde) != 0) {
        return -1;
    }
    if ((pde & MMU_P) != 0) {
        *pt_base = pde & 0xfffff000u;
        return 0;
    }
    if (u->next_pt > m->mem_size || MMU_PAGE_BYTES > m->mem_size - u->next_pt) {
        return -1;
    }
    if (zero_page(m, u->next_pt) != 0) {
        return -1;
    }
    pde = mmu_pack(u->next_pt >> 12, 1, 0, 0, 0);
    if (mm_raw_write(m, addr, 4, pde) != 0) {
        return -1;
    }
    *pt_base = u->next_pt;
    u->next_pt += MMU_PAGE_BYTES;
    return 0;
}

int mmu_map(MmMachine *m, uint32_t va, uint32_t pa, int w, int uflag) {
    MmuSplit sp;
    uint32_t pt = 0;
    uint32_t pte;
    if (!mmu_on(m) || (va & 0xfffu) != 0 || (pa & 0xfffu) != 0 || pa >= m->mem_size) {
        return -1;
    }
    mmu_split(va, &sp);
    if (ensure_pt(m, m->mmu, sp.dir, &pt) != 0) {
        return -1;
    }
    pte = mmu_pack(pa >> 12, w, uflag, 0, 0);
    return mm_raw_write(m, mmu_slot(pt, sp.page), 4, pte) == 0 ? 0 : -1;
}

int mmu_plant(MmMachine *m, int set, int way, int valid, uint32_t tag, uint32_t frame, int w, int uflag) {
    MmuLine *ln;
    if (!mmu_on(m) || set < 0 || set >= m->mmu->sets || way < 0 || way >= m->mmu->ways) {
        return -1;
    }
    ln = &m->mmu->line[set * m->mmu->ways + way];
    memset(ln, 0, sizeof *ln);
    ln->valid = valid ? 1 : 0;
    ln->tag = tag;
    ln->frame = frame;
    ln->w = w;
    ln->u = uflag;
    ln->vpn = (tag << m->mmu->index_bits) | (uint32_t)set;
    ln->stamp = ++m->mmu->clock;
    return 0;
}

void mmu_flush(MmMachine *m) {
    int i;
    if (!mmu_on(m)) {
        return;
    }
    for (i = 0; i < m->mmu->entries; i++) {
        m->mmu->line[i].valid = 0;
    }
}

void mmu_set_fix(MmMachine *m, uint32_t va, uint32_t pa, int w, int uflag) {
    if (!mmu_on(m)) {
        return;
    }
    m->mmu->fix_on = 1;
    m->mmu->fix_va = va;
    m->mmu->fix_pa = pa;
    m->mmu->fix_w = w;
    m->mmu->fix_u = uflag;
}

int mmu_repair(MmMachine *m) {
    uint32_t page;
    if (!mmu_on(m) || !m->mmu->fix_on) {
        return -1;
    }
    page = m->badva & ~0xfffu;
    if (page != (m->mmu->fix_va & ~0xfffu)) {
        return -1;
    }
    if (mmu_map(m, page, m->mmu->fix_pa, m->mmu->fix_w, m->mmu->fix_u) != 0) {
        return -1;
    }
    m->mmu->fix_on = 0;
    return 0;
}

static void cut_vpn(const Mmu *u, uint32_t vpn, int *set, uint32_t *tag) {
    uint32_t mask = u->index_bits == 0 ? 0u : (1u << u->index_bits) - 1u;
    *set = (int)(vpn & mask);
    *tag = vpn >> u->index_bits;
}

static int find_line(Mmu *u, uint32_t vpn, int *set, int *way) {
    uint32_t tag = 0;
    int i;
    cut_vpn(u, vpn, set, &tag);
    for (i = 0; i < u->ways; i++) {
        MmuLine *ln = &u->line[*set * u->ways + i];
        if (ln->valid && ln->tag == tag) {
            *way = i;
            return 1;
        }
    }
    return 0;
}

static int pick_way(Mmu *u, int set) {
    int base = set * u->ways;
    int way;
    int old = 0;
    for (way = 0; way < u->ways; way++) {
        if (!u->line[base + way].valid) {
            return way;
        }
    }
    if (u->replace == MMU_REPL_RAND) {
        u->seed = u->seed * 1103515245u + 12345u;
        return (int)((u->seed >> 16) % (uint32_t)u->ways);
    }
    for (way = 1; way < u->ways; way++) {
        if (u->line[base + way].stamp < u->line[base + old].stamp) {
            old = way;
        }
    }
    return old;
}

static void note_use(Mmu *u, MmuLine *ln, int filled) {
    if (filled || u->replace != MMU_REPL_FIFO) {
        ln->stamp = ++u->clock;
    }
}

static int denied(const MmuLine *ln, int writing, int user) {
    if (writing && !ln->w) {
        return 1;
    }
    return user && !ln->u;
}

static void write_ad(MmMachine *m, MmuLine *ln, int writing) {
    uint32_t raw = 0;
    ln->a = 1;
    if (writing) {
        ln->d = 1;
    }
    if (ln->pte_addr == 0 || read_u32(m, ln->pte_addr, &raw) != 0) {
        return;
    }
    raw |= MMU_A;
    if (writing) {
        raw |= MMU_D;
    }
    mm_raw_write(m, ln->pte_addr, 4, raw);
}

static const char *step_name(int kind) {
    if (kind == MMU_STEP_PROT) {
        return "越权";
    }
    if (kind == MMU_STEP_MCHK) {
        return "机器检查";
    }
    return "缺页";
}

static void trace_step(MmMachine *m, const MmuStep *s) {
    const char *how = s->tlb ? "命中" : "缺失";
    if (m->trace == NULL) {
        return;
    }
    if (s->kind == MMU_STEP_TLB || s->kind == MMU_STEP_PT) {
        fprintf(m->trace, "翻译 VA=%08X DIR=%X PAGE=%X OFF=%X TLB=%s PA=%08X\n", s->va, s->dir, s->page,
                s->offset, how, s->pa);
        return;
    }
    fprintf(m->trace, "翻译 VA=%08X DIR=%X PAGE=%X OFF=%X TLB=%s 结果=%s\n", s->va, s->dir, s->page, s->offset,
            how, step_name(s->kind));
}

static void charge(MmMachine *m, Mmu *u, int walks, int on) {
    u->walks += walks;
    if (on) {
        m->mmu_stall += walks * u->walk_cycles;
    }
}

static void load_bits(MmuLine *ln, uint32_t pte, uint32_t pte_addr) {
    ln->frame = pte >> 12;
    ln->pte_addr = pte_addr;
    ln->w = (pte & MMU_W) != 0;
    ln->u = (pte & MMU_U) != 0;
    ln->a = (pte & MMU_A) != 0;
    ln->d = (pte & MMU_D) != 0;
}

static MmuLine *fill_line(Mmu *u, uint32_t vpn, uint32_t pte, uint32_t pte_addr) {
    int set = 0;
    uint32_t tag = 0;
    int way;
    MmuLine *ln;
    cut_vpn(u, vpn, &set, &tag);
    way = pick_way(u, set);
    ln = &u->line[set * u->ways + way];
    memset(ln, 0, sizeof *ln);
    ln->valid = 1;
    ln->tag = tag;
    ln->vpn = vpn;
    load_bits(ln, pte, pte_addr);
    note_use(u, ln, 1);
    return ln;
}

static int finish(MmMachine *m, Mmu *u, MmuLine *ln, int writing, int user, MmuStep *out) {
    if (denied(ln, writing, user)) {
        out->kind = MMU_STEP_PROT;
        u->prot++;
        m->badva = out->va;
        return MM_EX_PROT;
    }
    if (out->tlb) {
        note_use(u, ln, 0);
        u->tlb_hits++;
    } else {
        u->pt_hits++;
    }
    write_ad(m, ln, writing);
    out->frame = ln->frame;
    out->pa = mmu_join(ln->frame, out->offset);
    if (ln->pte_addr != 0 && read_u32(m, ln->pte_addr, &out->pte) == 0) {
        out->saw_pte = 1;
        out->pte_addr = ln->pte_addr;
    }
    return 0;
}

static int from_tlb(MmMachine *m, Mmu *u, MmuLine *ln, int writing, int user, MmuStep *out) {
    out->tlb = 1;
    out->kind = MMU_STEP_TLB;
    return finish(m, u, ln, writing, user, out);
}

static int fail_page(MmMachine *m, Mmu *u, MmuStep *out, int walks, int on) {
    charge(m, u, walks, on);
    out->kind = MMU_STEP_FAULT;
    u->faults++;
    m->badva = out->va;
    return MM_EX_PAGE;
}

static int from_walk(MmMachine *m, Mmu *u, const MmuSplit *sp, int writing, int user, int on, MmuStep *out) {
    uint32_t pde = 0;
    uint32_t pte = 0;
    uint32_t pt_base;
    int walks = 0;
    MmuLine *ln;
    out->pde_addr = mmu_slot(u->ptbr, sp->dir);
    if (read_u32(m, out->pde_addr, &pde) != 0) {
        out->kind = MMU_STEP_MCHK;
        m->badva = out->va;
        return MM_EX_MCHK;
    }
    walks = 1;
    out->pde = pde;
    out->saw_pde = 1;
    if ((pde & MMU_P) == 0) {
        return fail_page(m, u, out, walks, on);
    }
    pt_base = pde & 0xfffff000u;
    out->pte_addr = mmu_slot(pt_base, sp->page);
    if (read_u32(m, out->pte_addr, &pte) != 0) {
        charge(m, u, walks, on);
        out->kind = MMU_STEP_MCHK;
        m->badva = out->va;
        return MM_EX_MCHK;
    }
    walks = 2;
    out->pte = pte;
    out->saw_pte = 1;
    charge(m, u, walks, on);
    if ((pte & MMU_P) == 0) {
        out->kind = MMU_STEP_FAULT;
        u->faults++;
        m->badva = out->va;
        return MM_EX_PAGE;
    }
    ln = fill_line(u, sp->vpn, pte, out->pte_addr);
    out->kind = MMU_STEP_PT;
    return finish(m, u, ln, writing, user, out);
}

static int prepare(MmMachine *m, uint32_t va, int writing, int on, MmuStep *out) {
    MmuSplit sp;
    int set = 0;
    int way = 0;
    int user;
    int rc;
    memset(out, 0, sizeof *out);
    out->cache = MMU_CACHE_NONE;
    if (!mmu_on(m) || (m->psw & MM_PSW_PG) == 0) {
        return -1;
    }
    mmu_split(va, &sp);
    out->va = va;
    out->dir = sp.dir;
    out->page = sp.page;
    out->offset = sp.offset;
    user = (m->psw & MM_PSW_U) != 0;
    m->mmu->accesses++;
    if (find_line(m->mmu, sp.vpn, &set, &way)) {
        rc = from_tlb(m, m->mmu, &m->mmu->line[set * m->mmu->ways + way], writing, user, out);
    } else {
        rc = from_walk(m, m->mmu, &sp, writing, user, on, out);
    }
    trace_step(m, out);
    return rc;
}

int mmu_xlate(MmMachine *m, uint32_t va, int writing, MmuStep *out) {
    return prepare(m, va, writing, 0, out);
}

static int cpu_touch(MmMachine *m, uint32_t va, int size, int writing, uint32_t *data) {
    MmuStep step;
    int vec = prepare(m, va, writing, 1, &step);
    if (vec != 0) {
        return vec;
    }
    if (writing) {
        return mm_write(m, step.pa, size, *data);
    }
    return mm_read(m, step.pa, size, data);
}

int mmu_cpu_read(MmMachine *m, uint32_t va, int size, uint32_t *data) {
    return cpu_touch(m, va, size, 0, data);
}

int mmu_cpu_write(MmMachine *m, uint32_t va, int size, uint32_t data) {
    return cpu_touch(m, va, size, 1, &data);
}

int mmu_mem_read(MmMachine *m, uint32_t addr, int size, uint32_t *data) {
    if (mmu_on(m) && (m->psw & MM_PSW_PG) != 0) {
        return mmu_cpu_read(m, addr, size, data);
    }
    return mm_read(m, addr, size, data);
}

int mmu_mem_write(MmMachine *m, uint32_t addr, int size, uint32_t data) {
    if (mmu_on(m) && (m->psw & MM_PSW_PG) != 0) {
        return mmu_cpu_write(m, addr, size, data);
    }
    return mm_write(m, addr, size, data);
}

int mmu_take_stall(MmMachine *m) {
    int n;
    if (m == NULL) {
        return 0;
    }
    n = m->mmu_stall;
    m->mmu_stall = 0;
    return n;
}

void mmu_stat(const MmMachine *m, MmuStat *out) {
    memset(out, 0, sizeof *out);
    if (!mmu_on(m)) {
        return;
    }
    out->accesses = m->mmu->accesses;
    out->tlb_hits = m->mmu->tlb_hits;
    out->pt_hits = m->mmu->pt_hits;
    out->faults = m->mmu->faults;
    out->prot = m->mmu->prot;
    out->walks = m->mmu->walks;
}

void mmu_report(const MmMachine *m, FILE *fp) {
    MmuStat st;
    if (!mmu_on(m)) {
        return;
    }
    mmu_stat(m, &st);
    fprintf(fp, "MMU 访问=%d 快表命中=%d 页表命中=%d 缺页=%d 越权=%d 查表=%d\n", st.accesses, st.tlb_hits,
            st.pt_hits, st.faults, st.prot, st.walks);
}
