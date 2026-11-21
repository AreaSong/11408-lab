#include "common/mm_shell_stage.h"

#include "common/mm_internal.h"
#include "mmu/mmu_fmt.h"
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

static void say_eat(FILE *out, const char *title, unsigned long long num, unsigned long long den) {
    fprintf(out, "【有效访问时间】%s\n", title);
    if (den == 1) {
        fprintf(out, "结果：%llu ns\n", num);
    } else {
        fprintf(out, "结果：%llu/%llu ns\n", num, den);
    }
}

static int eat_done(FILE *out, const char *title, int rc, unsigned long long num, unsigned long long den) {
    if (rc != 0) {
        fprintf(out, "错误：eat 的数不合法\n");
        return -1;
    }
    say_eat(out, title, num, den);
    return 1;
}

static int eat_tlb(const char *arg, FILE *out) {
    uint32_t a = 0;
    uint32_t b = 0;
    unsigned long long num = 0;
    unsigned long long den = 0;
    char title[96];
    const char *p = arg;
    if (next_u32(&p, &a) != 1 || next_u32(&p, &b) != 1) {
        fprintf(out, "错误：eat tlb-hit 快表ns 内存ns\n");
        return -1;
    }
    snprintf(title, sizeof title, "快表命中，快表 %u ns + 内存 %u ns", a, b);
    return eat_done(out, title, mmu_eat(MMU_EAT_TLB, 0, 0, a, b, 1, 0, &num, &den), num, den);
}

static int eat_pt(const char *arg, FILE *out) {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    unsigned long long num = 0;
    unsigned long long den = 0;
    char title[96];
    const char *p = arg;
    if (next_u32(&p, &a) != 1 || next_u32(&p, &b) != 1 || next_u32(&p, &c) != 1) {
        fprintf(out, "错误：eat pt-hit 快表ns 内存ns 查表次数\n");
        return -1;
    }
    snprintf(title, sizeof title, "页表命中，查表 %u 次", c);
    return eat_done(out, title, mmu_eat(MMU_EAT_PT, 0, 0, a, b, (int)c, 0, &num, &den), num, den);
}

static int eat_fault(const char *arg, FILE *out) {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    unsigned long long num = 0;
    unsigned long long den = 0;
    char title[96];
    const char *p = arg;
    if (next_u32(&p, &a) != 1 || next_u32(&p, &b) != 1 || next_u32(&p, &c) != 1 || next_u32(&p, &d) != 1) {
        fprintf(out, "错误：eat fault 快表ns 内存ns 查表次数 缺页ns\n");
        return -1;
    }
    snprintf(title, sizeof title, "缺页后重执行，查表 %u 次", c);
    return eat_done(out, title, mmu_eat(MMU_EAT_FAULT, 0, 0, a, b, (int)c, d, &num, &den), num, den);
}

static int eat_avg(const char *arg, FILE *out) {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    uint32_t e = 0;
    unsigned long long num = 0;
    unsigned long long den = 0;
    char title[96];
    const char *p = arg;
    if (next_u32(&p, &a) != 1 || next_u32(&p, &b) != 1 || next_u32(&p, &c) != 1 || next_u32(&p, &d) != 1 ||
        next_u32(&p, &e) != 1) {
        fprintf(out, "错误：eat avg 命中数 分母 快表ns 内存ns 查表次数\n");
        return -1;
    }
    snprintf(title, sizeof title, "平均，命中 %u/%u，查表 %u 次", a, b, e);
    return eat_done(out, title, mmu_eat(MMU_EAT_AVG, a, b, c, d, (int)e, 0, &num, &den), num, den);
}

static int cmd_eat(const char *arg, FILE *out) {
    if (starts(arg, "tlb-hit")) {
        return eat_tlb(rest_of(arg, "tlb-hit"), out);
    }
    if (starts(arg, "pt-hit")) {
        return eat_pt(rest_of(arg, "pt-hit"), out);
    }
    if (starts(arg, "fault")) {
        return eat_fault(rest_of(arg, "fault"), out);
    }
    if (starts(arg, "avg")) {
        return eat_avg(rest_of(arg, "avg"), out);
    }
    fprintf(out, "错误：eat 只能是 tlb-hit、pt-hit、fault 或 avg\n");
    return -1;
}

static int cmd_geom(const char *arg, FILE *out) {
    uint32_t va = 0;
    uint32_t page = 0;
    uint32_t pte = 0;
    MmuGeom g;
    const char *p = arg;
    if (next_u32(&p, &va) != 1 || next_u32(&p, &page) != 1 || next_u32(&p, &pte) != 1 ||
        mmu_geom((int)va, page, pte, &g) != 0) {
        fprintf(out, "错误：geom 地址位数 页字节 项字节\n");
        return -1;
    }
    fprintf(out, "【页表规模】虚拟地址 %u 位，页 %u B，页表项 %u B\n", va, page, pte);
    fprintf(out, "单级 %llu 项、%llu B\n", g.pages, g.table_bytes);
    fprintf(out, "每页 %llu 项，每级 %d 位，共 %d 级\n", g.per_page, g.level_bits, g.levels);
    fprintf(out, "顶级 %d 位、%llu 项、%llu B，%s\n", g.top_bits, g.top_entries, g.top_bytes,
            g.fits ? "不超过一页" : "超过一页");
    fprintf(out, "结果：顶级项 = %llu，页表页 = %llu，顶级%s一页\n", g.top_entries, g.tables_pages,
            g.fits ? "不超过" : "超过");
    return 1;
}

static int cmd_dirs(const char *arg, FILE *out) {
    uint32_t dbits = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t da = 0;
    uint32_t db = 0;
    const char *p = arg;
    if (next_u32(&p, &dbits) != 1 || next_u32(&p, &a) != 1 || next_u32(&p, &b) != 1) {
        fprintf(out, "错误：dirs 目录位数 地址1 地址2\n");
        return -1;
    }
    if (mmu_top(a, (int)dbits, &da) != 0 || mmu_top(b, (int)dbits, &db) != 0) {
        fprintf(out, "错误：dirs 的目录位数要在 1 到 31\n");
        return -1;
    }
    fprintf(out, "【二级页表】%08XH 的页目录号 = %u，%08XH 的页目录号 = %u\n", a, da, b, db);
    fprintf(out, "结果：二级页表 = %u 个\n", da == db ? 1u : 2u);
    return 1;
}

static int cmd_slot(const char *arg, FILE *out) {
    uint32_t base = 0;
    uint32_t index = 0;
    const char *p = arg;
    if (next_u32(&p, &base) != 1 || next_u32(&p, &index) != 1) {
        fprintf(out, "错误：slot 基址 下标\n");
        return -1;
    }
    fprintf(out, "【表项地址】基址 %08XH + 下标 %u × 4\n", base, index);
    fprintf(out, "结果：表项地址 = %08XH\n", mmu_slot(base, index));
    return 1;
}

static int cmd_join(const char *arg, FILE *out) {
    uint32_t frame = 0;
    uint32_t offset = 0;
    const char *p = arg;
    if (next_u32(&p, &frame) != 1 || next_u32(&p, &offset) != 1 || offset >= MMU_PAGE_BYTES) {
        fprintf(out, "错误：join 页框号 页内偏移\n");
        return -1;
    }
    fprintf(out, "【物理地址】页框 %XH，偏移 %XH\n", frame, offset);
    fprintf(out, "结果：物理地址 = %08XH\n", mmu_join(frame, offset));
    return 1;
}

static int cmd_pack(const char *arg, FILE *out) {
    uint32_t dir = 0;
    uint32_t page = 0;
    uint32_t offset = 0;
    const char *p = arg;
    if (next_u32(&p, &dir) != 1 || next_u32(&p, &page) != 1 || next_u32(&p, &offset) != 1 || dir > 0x3ffu ||
        page > 0x3ffu || offset >= MMU_PAGE_BYTES) {
        fprintf(out, "错误：pack 页目录号 页号 页内偏移\n");
        return -1;
    }
    fprintf(out, "【拼地址】页目录号 %u，页号 %u，页内偏移 %u\n", dir, page, offset);
    fprintf(out, "结果：虚拟地址 = %08XH\n", mmu_pack_va(dir, page, offset));
    return 1;
}

static int cmd_cut(const char *arg, FILE *out) {
    uint32_t va = 0;
    MmuSplit sp;
    if (parse_u32(arg, &va) != 0) {
        fprintf(out, "错误：cut 虚拟地址\n");
        return -1;
    }
    mmu_split(va, &sp);
    fprintf(out, "【拆地址】%08XH\n", va);
    fprintf(out, "结果：页目录号 = %u，页号 = %u，页内偏移 = %u\n", sp.dir, sp.page, sp.offset);
    return 1;
}

static int cmd_elem(const char *arg, FILE *out) {
    uint32_t base = 0;
    uint32_t cols = 0;
    uint32_t width = 0;
    uint32_t row = 0;
    uint32_t col = 0;
    uint32_t ptbr = 0;
    uint32_t frame = 0;
    uint32_t va;
    uint32_t pde;
    uint32_t pte;
    MmuSplit sp;
    const char *p = arg;
    if (next_u32(&p, &base) != 1 || next_u32(&p, &cols) != 1 || next_u32(&p, &width) != 1 || next_u32(&p, &row) != 1 ||
        next_u32(&p, &col) != 1 || next_u32(&p, &ptbr) != 1 || next_u32(&p, &frame) != 1 || cols == 0 || col >= cols) {
        fprintf(out, "错误：elem 起始 列数 元素字节 行 列 页目录地址 页框号\n");
        return -1;
    }
    va = base + (row * cols + col) * width;
    mmu_split(va, &sp);
    pde = mmu_slot(ptbr, sp.dir);
    pte = mmu_slot(mmu_join(frame, 0), sp.page);
    fprintf(out, "【数组元素】a[%u][%u] 的虚拟地址 = %08XH\n", row, col, va);
    fprintf(out, "页目录号 = %u（%XH），页号 = %u（%XH），页内偏移 = %u\n", sp.dir, sp.dir, sp.page, sp.page,
            sp.offset);
    fprintf(out, "页目录项地址 = %08XH\n", pde);
    fprintf(out, "页表项地址 = %08XH\n", pte);
    fprintf(out, "结果：虚拟地址 = %08XH，页目录项 = %08XH，页表项 = %08XH\n", va, pde, pte);
    return 1;
}

static int read_slots(const char **p, MmuTlbItem *item, int entries, FILE *out) {
    int i;
    for (i = 0; i < entries; i++) {
        uint32_t valid = 0;
        uint32_t tag = 0;
        uint32_t frame = 0;
        if (next_u32(p, &valid) != 1 || next_u32(p, &tag) != 1 || next_u32(p, &frame) != 1) {
            fprintf(out, "错误：look 的快表项不够\n");
            return -1;
        }
        item[i].valid = (int)valid;
        item[i].tag = tag;
        item[i].frame = frame;
    }
    return 0;
}

static int cmd_look(const char *arg, FILE *out) {
    uint32_t va = 0;
    uint32_t entries = 0;
    uint32_t ways = 0;
    uint32_t frame = 0;
    MmuTlbItem item[64];
    const char *p = arg;
    int hit;
    memset(item, 0, sizeof item);
    if (next_u32(&p, &va) != 1 || next_u32(&p, &entries) != 1 || next_u32(&p, &ways) != 1 || entries == 0 ||
        entries > 64) {
        fprintf(out, "错误：look 虚拟地址 项数 路数 然后每项「有效 标记 页框」\n");
        return -1;
    }
    if (read_slots(&p, item, (int)entries, out) != 0) {
        return -1;
    }
    hit = mmu_tlb_find((int)entries, (int)ways, va >> MMU_OFF_BITS, item, (int)entries, &frame);
    fprintf(out, "【快表】虚拟地址 %08XH，虚页号 %XH，页内偏移 %XH\n", va, va >> MMU_OFF_BITS, va & 0xfffu);
    if (hit < 0) {
        fprintf(out, "错误：TLB 参数不是 2 的幂\n");
        return -1;
    }
    if (hit == 0) {
        fprintf(out, "结果：TLB 缺失\n");
        return 1;
    }
    fprintf(out, "结果：TLB 命中，物理地址 = %08XH\n", mmu_join(frame, va & 0xfffu));
    return 1;
}

static int read_vpns(const char **p, uint32_t *vpn, int *n, FILE *out) {
    *n = 0;
    while (*n < 64) {
        int got = next_u32(p, &vpn[*n]);
        if (got == 0) {
            return 0;
        }
        if (got < 0) {
            fprintf(out, "错误：series 的虚页号读不出来\n");
            return -1;
        }
        (*n)++;
    }
    return 0;
}

static int cmd_series(const char *arg, FILE *out) {
    uint32_t entries = 0;
    uint32_t ways = 0;
    uint32_t vpn[64];
    uint32_t victim = 0;
    int n = 0;
    int hits = 0;
    int rc;
    const char *p = arg;
    if (next_u32(&p, &entries) != 1 || next_u32(&p, &ways) != 1) {
        fprintf(out, "错误：series 项数 路数 虚页号…\n");
        return -1;
    }
    if (read_vpns(&p, vpn, &n, out) != 0) {
        return -1;
    }
    rc = mmu_tlb_series((int)entries, (int)ways, vpn, n, &hits, &victim);
    if (rc < 0) {
        fprintf(out, "错误：series 的项数和路数要是 2 的幂\n");
        return -1;
    }
    fprintf(out, "【TLB 替换】%u 项，每组 %u 路，LRU，空行优先\n", entries, ways);
    if (rc == 0) {
        fprintf(out, "结果：没有替换，命中 %d 次\n", hits);
        return 1;
    }
    fprintf(out, "结果：被替换的虚页号 = %u，命中 %d 次\n", victim, hits);
    return 1;
}

int mm_shell_calc(const char *line, FILE *out) {
    if (starts(line, "geom")) {
        return cmd_geom(rest_of(line, "geom"), out);
    }
    if (starts(line, "dirs")) {
        return cmd_dirs(rest_of(line, "dirs"), out);
    }
    if (starts(line, "slot")) {
        return cmd_slot(rest_of(line, "slot"), out);
    }
    if (starts(line, "join")) {
        return cmd_join(rest_of(line, "join"), out);
    }
    if (starts(line, "pack")) {
        return cmd_pack(rest_of(line, "pack"), out);
    }
    if (starts(line, "cut")) {
        return cmd_cut(rest_of(line, "cut"), out);
    }
    if (starts(line, "elem")) {
        return cmd_elem(rest_of(line, "elem"), out);
    }
    if (starts(line, "look")) {
        return cmd_look(rest_of(line, "look"), out);
    }
    if (starts(line, "series")) {
        return cmd_series(rest_of(line, "series"), out);
    }
    if (starts(line, "eat")) {
        return cmd_eat(rest_of(line, "eat"), out);
    }
    return 0;
}
