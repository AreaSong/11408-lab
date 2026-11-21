#include "common/mm_shell_stage.h"

#include "common/mm_internal.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_tlb.h"
#include "mmu/mmu_walk.h"

#include <stdio.h>
#include <string.h>

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest_of(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int parse_u32(const char *text, uint32_t *out) {
    int64_t num = 0;
    char err[64];
    if (mm_parse_i64(text, &num, err, sizeof err) != 0 || num < 0 || num > 4294967295LL) {
        return -1;
    }
    *out = (uint32_t)num;
    return 0;
}

static int next_u32(const char **p, uint32_t *out) {
    char tok[64];
    int n = 0;
    const char *s = *p;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0' && *s != ' ' && *s != '\t' && n < 63) {
        tok[n++] = *s++;
    }
    tok[n] = '\0';
    if (parse_u32(tok, out) != 0) {
        return -1;
    }
    *p = s;
    return 1;
}

static int leading_numbers(const char *arg) {
    const char *p = arg;
    int n = 0;
    uint32_t value = 0;
    while (n < 6) {
        int got = next_u32(&p, &value);
        if (got != 1) {
            return n;
        }
        n++;
    }
    return n;
}

static MmMachine *need_machine(MmMachine **slot, uint32_t bytes) {
    if (*slot == NULL) {
        *slot = mm_create(bytes);
    }
    return *slot;
}

static const char *repl_name(int repl) {
    if (repl == MMU_REPL_FIFO) {
        return "FIFO";
    }
    if (repl == MMU_REPL_RAND) {
        return "随机";
    }
    return "LRU";
}

void mm_shell_mmu_apply(MmMachine *m, const MmShellOpt *opt) {
    MmuCfg cfg;
    int i;
    uint32_t psw;
    if (m == NULL) {
        return;
    }
    psw = mm_psw(m) & ~MM_PSW_PG;
    if (!opt->pg) {
        mmu_drop(m);
        mm_set_psw(m, psw);
        return;
    }
    cfg.entries = opt->tlb_entries;
    cfg.ways = opt->tlb_ways;
    cfg.replace = opt->tlb_repl;
    cfg.walk_cycles = opt->walk_cycles;
    cfg.ptbr = opt->ptbr;
    if (mmu_enable(m, &cfg) != 0) {
        fprintf(stderr, "错误：MMU 参数或页目录放不进主存\n");
        mm_set_psw(m, psw);
        return;
    }
    mm_set_psw(m, psw | MM_PSW_PG);
    for (i = 0; i < opt->map_n; i++) {
        mmu_map(m, opt->map_va[i], opt->map_pa[i], opt->map_w[i], opt->map_u[i]);
    }
    for (i = 0; i < opt->plant_n; i++) {
        mmu_plant(m, opt->plant_set[i], opt->plant_way[i], opt->plant_valid[i], opt->plant_tag[i],
                  opt->plant_frame[i], opt->plant_w[i], opt->plant_u[i]);
    }
    if (opt->fix_on) {
        mmu_set_fix(m, opt->fix_va, opt->fix_pa, 1, 1);
    }
}

static void say_pg(FILE *out, const MmShellOpt *opt) {
    if (!opt->pg) {
        fprintf(out, "分页 off\n");
        return;
    }
    fprintf(out, "分页 on 页目录=%08XH TLB=%d 项 %d 路 %s 查表周期=%d\n", opt->ptbr, opt->tlb_entries,
            opt->tlb_ways, repl_name(opt->tlb_repl), opt->walk_cycles);
}

static int cmd_pg(const char *arg, FILE *out, MmMachine *m, MmShellOpt *opt) {
    if (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0) {
        fprintf(out, "错误：pg 只能是 on 或 off\n");
        return -1;
    }
    opt->pg = strcmp(arg, "on") == 0;
    if (m != NULL) {
        mm_shell_mmu_apply(m, opt);
    }
    say_pg(out, opt);
    return 1;
}

static int add_map(MmShellOpt *opt, uint32_t va, uint32_t pa, int w, int u) {
    if (opt->map_n >= 16) {
        return -1;
    }
    opt->map_va[opt->map_n] = va;
    opt->map_pa[opt->map_n] = pa;
    opt->map_w[opt->map_n] = (uint8_t)w;
    opt->map_u[opt->map_n] = (uint8_t)u;
    opt->map_n++;
    return 0;
}

static int cmd_map(const char *arg, FILE *out, MmMachine *m, MmShellOpt *opt) {
    uint32_t va = 0;
    uint32_t pa = 0;
    char acc[8];
    char who[8];
    int w = 1;
    int u = 1;
    int n = sscanf(arg, "%*s %*s %7s %7s", acc, who);
    const char *p = arg;
    if (next_u32(&p, &va) != 1 || next_u32(&p, &pa) != 1) {
        fprintf(out, "错误：map 虚拟地址 物理地址 rw|ro user|kernel\n");
        return -1;
    }
    if (n == 2) {
        w = strcmp(acc, "ro") != 0;
        u = strcmp(who, "kernel") != 0;
    }
    if (add_map(opt, va, pa, w, u) != 0) {
        fprintf(out, "错误：映射太多\n");
        return -1;
    }
    if (m != NULL && opt->pg) {
        mm_shell_mmu_apply(m, opt);
    }
    fprintf(out, "映射 VA=%08XH PA=%08XH W=%d U=%d\n", va, pa, w, u);
    return 1;
}

static int cmd_ident(const char *arg, FILE *out, MmMachine *m, MmShellOpt *opt) {
    uint32_t start = 0;
    uint32_t bytes = 0;
    uint32_t at;
    const char *p = arg;
    if (next_u32(&p, &start) != 1 || next_u32(&p, &bytes) != 1 || (start & 0xfffu) != 0 || bytes == 0) {
        fprintf(out, "错误：ident 页对齐起址 字节数\n");
        return -1;
    }
    for (at = start; at < start + bytes; at += MMU_PAGE_BYTES) {
        if (add_map(opt, at, at, 1, 1) != 0) {
            fprintf(out, "错误：映射太多\n");
            return -1;
        }
    }
    if (m != NULL && opt->pg) {
        mm_shell_mmu_apply(m, opt);
    }
    fprintf(out, "恒等映射 %08XH 起 %u 字节\n", start, bytes);
    return 1;
}

static int cmd_tlb(const char *arg, FILE *out, MmMachine *m, MmShellOpt *opt) {
    const char *p = arg;
    uint32_t entries = 0;
    uint32_t ways = 0;
    int sets = 0;
    int bits = 0;
    if (starts(arg, "replace")) {
        const char *name = rest_of(arg, "replace");
        opt->tlb_repl = strcmp(name, "fifo") == 0 ? MMU_REPL_FIFO : strcmp(name, "rand") == 0 ? MMU_REPL_RAND
                                                                                              : MMU_REPL_LRU;
        if (strcmp(name, "lru") != 0 && strcmp(name, "fifo") != 0 && strcmp(name, "rand") != 0) {
            fprintf(out, "错误：tlb replace 只能是 lru、fifo 或 rand\n");
            return -1;
        }
    } else if (starts(arg, "walk")) {
        if (next_u32(&p, &entries) != 0) {
            p = rest_of(arg, "walk");
        }
        if (next_u32(&p, &entries) != 1 || entries == 0) {
            fprintf(out, "错误：tlb walk 需要周期数\n");
            return -1;
        }
        opt->walk_cycles = (int)entries;
    } else if (strcmp(arg, "flush") == 0) {
        mmu_flush(m);
        fprintf(out, "TLB 已清空\n");
        return 1;
    } else if (next_u32(&p, &entries) != 1 || next_u32(&p, &ways) != 1 ||
               mmu_tlb_geom((int)entries, (int)ways, &sets, &bits) != 0) {
        fprintf(out, "错误：tlb 项数 路数，都要是 2 的幂，路数能整除项数\n");
        return -1;
    } else {
        opt->tlb_entries = (int)entries;
        opt->tlb_ways = (int)ways;
    }
    if (m != NULL && opt->pg) {
        mm_shell_mmu_apply(m, opt);
    }
    say_pg(out, opt);
    return 1;
}

static int cmd_plant(const char *arg, FILE *out, MmMachine *m, MmShellOpt *opt) {
    uint32_t set = 0;
    uint32_t way = 0;
    uint32_t valid = 0;
    uint32_t tag = 0;
    uint32_t frame = 0;
    const char *p = arg;
    int i;
    if (opt->plant_n >= 8 || next_u32(&p, &set) != 1 || next_u32(&p, &way) != 1 || next_u32(&p, &valid) != 1 ||
        next_u32(&p, &tag) != 1 || next_u32(&p, &frame) != 1) {
        fprintf(out, "错误：plant 组 路 有效 标记 页框\n");
        return -1;
    }
    i = opt->plant_n++;
    opt->plant_set[i] = (uint8_t)set;
    opt->plant_way[i] = (uint8_t)way;
    opt->plant_valid[i] = (uint8_t)valid;
    opt->plant_tag[i] = tag;
    opt->plant_frame[i] = frame;
    opt->plant_w[i] = 1;
    opt->plant_u[i] = 1;
    if (m != NULL && opt->pg) {
        mm_shell_mmu_apply(m, opt);
    }
    fprintf(out, "TLB 预置 组 %u 路 %u 有效 %u 标记 %XH 页框 %XH\n", set, way, valid, tag, frame);
    return 1;
}

static int cmd_fix(const char *arg, FILE *out, MmMachine *m, MmShellOpt *opt) {
    uint32_t va = 0;
    uint32_t pa = 0;
    const char *p = arg;
    if (next_u32(&p, &va) != 1 || next_u32(&p, &pa) != 1) {
        fprintf(out, "错误：fix 虚拟页 物理页\n");
        return -1;
    }
    opt->fix_on = 1;
    opt->fix_va = va;
    opt->fix_pa = pa;
    if (m != NULL && opt->pg) {
        mm_shell_mmu_apply(m, opt);
    }
    fprintf(out, "缺页修复 VA=%08XH PA=%08XH\n", va, pa);
    return 1;
}


static const char *cache_name(int cache) {
    if (cache == MMU_CACHE_HIT) {
        return "命中";
    }
    if (cache == MMU_CACHE_MISS) {
        return "缺失";
    }
    if (cache == MMU_CACHE_RANGE) {
        return "越界";
    }
    return "直通";
}

static void touch_cache(MmMachine *m, const MmuStep *step, int writing, int *cache) {
    MemCacheStat before;
    MemCacheStat after;
    uint32_t word = 0;
    *cache = MMU_CACHE_OFF;
    if (!mem_cache_on(m)) {
        return;
    }
    if (mm_raw_read(m, step->pa, 4, &word) != 0) {
        *cache = MMU_CACHE_RANGE;
        return;
    }
    mem_cache_stat(m, 0, &before);
    if (writing) {
        mm_write(m, step->pa, 4, word);
    } else {
        mm_read(m, step->pa, 4, &word);
    }
    mem_cache_stat(m, 0, &after);
    *cache = after.hits > before.hits ? MMU_CACHE_HIT : MMU_CACHE_MISS;
}

static void say_step(FILE *out, const MmuStep *s, int writing) {
    int p = 0;
    int w = 0;
    int u = 0;
    int a = 0;
    int d = 0;
    uint32_t frame = 0;
    fprintf(out, "【地址翻译】%s VA=%08XH\n", writing ? "写" : "读", s->va);
    fprintf(out, "页目录号 = %u，页号 = %u，页内偏移 = %u\n", s->dir, s->page, s->offset);
    fprintf(out, "TLB %s\n", s->tlb ? "命中" : "缺失");
    if (s->saw_pde) {
        mmu_fields(s->pde, &p, &w, &u, &a, &d, &frame);
        fprintf(out, "页目录项地址 = %08XH，内容 = %08XH，P = %d\n", s->pde_addr, s->pde, p);
        if (p) {
            fprintf(out, "页表始址 = %08XH\n", frame << 12);
        }
    }
    if (s->saw_pte) {
        mmu_fields(s->pte, &p, &w, &u, &a, &d, &frame);
        fprintf(out, "页表项地址 = %08XH，内容 = %08XH，P = %d W = %d U = %d A = %d D = %d\n", s->pte_addr, s->pte,
                p, w, u, a, d);
    }
    if (s->kind == MMU_STEP_FAULT) {
        fprintf(out, "结果：缺页\n");
        return;
    }
    if (s->kind == MMU_STEP_PROT) {
        fprintf(out, "结果：越权\n");
        return;
    }
    if (s->kind == MMU_STEP_MCHK) {
        fprintf(out, "结果：机器检查\n");
        return;
    }
    fprintf(out, "物理地址 = %08XH，Cache %s\n", s->pa, cache_name(s->cache));
    fprintf(out, "结果：物理地址 = %08XH\n", s->pa);
}

static int cmd_xlate(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    uint32_t va = 0;
    int writing = starts(arg, "写");
    const char *p = rest_of(arg, writing ? "写" : "读");
    MmMachine *m;
    MmuStep step;
    int rc;
    if ((writing || starts(arg, "读")) && next_u32(&p, &va) == 1 && opt->pg) {
        m = need_machine(machine, bytes);
        if (m == NULL) {
            return -1;
        }
        if (!mmu_on(m)) {
            mm_shell_apply(m, opt);
        }
        rc = mmu_xlate(m, va, writing, &step);
        if (rc == 0 && (step.pa & 3u) == 0) {
            touch_cache(m, &step, writing, &step.cache);
        }
        say_step(out, &step, writing);
        return 1;
    }
    fprintf(out, "错误：xlate 读|写 虚拟地址，并且先 pg on\n");
    return -1;
}

int mm_shell_mmu(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt) {
    MmMachine *m = *machine;
    int calc = mm_shell_calc(line, out);
    if (calc != 0) {
        return calc;
    }
    if (starts(line, "pg")) {
        return cmd_pg(rest_of(line, "pg"), out, m, opt);
    }
    if (starts(line, "ptbr")) {
        uint32_t addr = 0;
        if (parse_u32(rest_of(line, "ptbr"), &addr) != 0) {
            fprintf(out, "错误：ptbr 物理地址\n");
            return -1;
        }
        opt->ptbr = addr;
        if (m != NULL && opt->pg) {
            mm_shell_mmu_apply(m, opt);
        }
        say_pg(out, opt);
        return 1;
    }
    if (starts(line, "tlb")) {
        return cmd_tlb(rest_of(line, "tlb"), out, m, opt);
    }
    if (starts(line, "map")) {
        /* 阶段 03 的 map 有五个数：地址位数、行数、路数、块字节、地址。 */
        if (leading_numbers(rest_of(line, "map")) >= 3) {
            return 0;
        }
        return cmd_map(rest_of(line, "map"), out, m, opt);
    }
    if (starts(line, "ident")) {
        return cmd_ident(rest_of(line, "ident"), out, m, opt);
    }
    if (starts(line, "plant")) {
        return cmd_plant(rest_of(line, "plant"), out, m, opt);
    }
    if (starts(line, "fix")) {
        return cmd_fix(rest_of(line, "fix"), out, m, opt);
    }
    if (starts(line, "xlate")) {
        return cmd_xlate(rest_of(line, "xlate"), out, machine, mem_bytes, opt);
    }
    return 0;
}
