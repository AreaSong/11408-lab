#include "common/mm_shell_stage.h"

#include "common/mm_internal.h"
#include "mem/mem_bank.h"
#include "mem/mem_cache.h"
#include "mem/mem_chip.h"

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

static int next_num(const char **p, uint32_t *out) {
    char buf[32];
    int n = 0;
    int64_t num = 0;
    char err[64];
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
    if (**p == '\0') {
        return 0;
    }
    while (**p != '\0' && **p != ' ' && **p != '\t' && n < 31) {
        buf[n++] = *(*p)++;
    }
    buf[n] = '\0';
    if (mm_parse_i64(buf, &num, err, sizeof err) != 0 || num < 0 || num > 4294967295LL) {
        return -1;
    }
    *out = (uint32_t)num;
    return 1;
}

static void fill_cfg(MemCacheCfg *cfg, const MmShellOpt *opt) {
    mem_cache_cfg_default(cfg);
    cfg->on = opt->l1_on;
    cfg->lines = opt->l1_lines;
    cfg->ways = opt->l1_ways;
    cfg->block_bytes = opt->l1_block;
    cfg->replace = opt->l1_replace;
    cfg->write_back = opt->l1_wb;
    cfg->write_alloc = opt->l1_alloc;
    cfg->mem_cycles = opt->l1_mem_cycles;
    cfg->split = opt->l1_split;
    cfg->seed = opt->l1_seed;
}

void mm_shell_mem_apply(MmMachine *m, const MmShellOpt *opt) {
    MemCacheCfg cfg;
    if (m == NULL) {
        return;
    }
    fill_cfg(&cfg, opt);
    if (mem_cache_set(m, &cfg) != 0) {
        fprintf(stderr, "错误：Cache 参数和主存对不上\n");
    }
}

static void say_l1(FILE *out, const MmShellOpt *opt) {
    const char *repl = "LRU";
    if (!opt->l1_on) {
        fprintf(out, "Cache 关\n");
        return;
    }
    if (opt->l1_replace == MEM_REPL_FIFO) {
        repl = "FIFO";
    } else if (opt->l1_replace == MEM_REPL_RAND) {
        repl = "随机";
    }
    fprintf(out, "Cache 开 行 %d 路 %d 块 %d %s %s %s 主存周期 %d %s\n", opt->l1_lines, opt->l1_ways, opt->l1_block,
            opt->l1_wb ? "写回" : "写直达", opt->l1_alloc ? "写分配" : "非写分配", repl, opt->l1_mem_cycles,
            opt->l1_split ? "分开" : "统一");
}

static int push_l1(MmMachine *m, MmShellOpt *opt, FILE *out) {
    MemCacheCfg cfg;
    fill_cfg(&cfg, opt);
    cfg.on = 1;
    if (!mem_cache_cfg_ok(&cfg)) {
        opt->l1_on = 0;
        fprintf(out, "错误：Cache 的行数、路数、块大小要是 2 的幂，块至少 4 字节\n");
        return -1;
    }
    cfg.on = opt->l1_on;
    if (m != NULL && mem_cache_set(m, &cfg) != 0) {
        opt->l1_on = 0;
        fprintf(out, "错误：Cache 参数和主存对不上\n");
        return -1;
    }
    say_l1(out, opt);
    return 1;
}

static int cmd_l1_num(const char *arg, MmMachine *m, MmShellOpt *opt, FILE *out) {
    const char *key = starts(arg, "lines") ? "lines" : starts(arg, "ways") ? "ways" : starts(arg, "block") ? "block"
                                                                              : starts(arg, "mem") ? "mem" : "seed";
    const char *rest = rest_of(arg, key);
    uint32_t num = 0;
    if (next_num(&rest, &num) != 1 || num == 0) {
        fprintf(out, "错误：l1 %s 需要正整数\n", key);
        return -1;
    }
    if (strcmp(key, "lines") == 0) {
        opt->l1_lines = (int)num;
    } else if (strcmp(key, "ways") == 0) {
        opt->l1_ways = (int)num;
    } else if (strcmp(key, "block") == 0) {
        opt->l1_block = (int)num;
    } else if (strcmp(key, "mem") == 0) {
        opt->l1_mem_cycles = (int)num;
    } else {
        opt->l1_seed = num;
    }
    return push_l1(m, opt, out);
}

static int cmd_l1(char *line, FILE *out, MmMachine *m, MmShellOpt *opt) {
    const char *arg = rest_of(line, "l1");
    if (strcmp(arg, "on") == 0 || strcmp(arg, "off") == 0) {
        opt->l1_on = strcmp(arg, "on") == 0;
        return push_l1(m, opt, out);
    }
    if (strcmp(arg, "split") == 0 || strcmp(arg, "unified") == 0) {
        opt->l1_split = strcmp(arg, "split") == 0;
        return push_l1(m, opt, out);
    }
    if (starts(arg, "lines") || starts(arg, "ways") || starts(arg, "block") || starts(arg, "mem") ||
        starts(arg, "seed")) {
        return cmd_l1_num(arg, m, opt, out);
    }
    if (starts(arg, "replace")) {
        const char *name = rest_of(arg, "replace");
        if (strcmp(name, "lru") == 0) {
            opt->l1_replace = MEM_REPL_LRU;
        } else if (strcmp(name, "fifo") == 0) {
            opt->l1_replace = MEM_REPL_FIFO;
        } else if (strcmp(name, "rand") == 0) {
            opt->l1_replace = MEM_REPL_RAND;
        } else {
            fprintf(out, "错误：l1 replace 只能是 lru、fifo 或 rand\n");
            return -1;
        }
        return push_l1(m, opt, out);
    }
    if (starts(arg, "write")) {
        const char *name = rest_of(arg, "write");
        if (strcmp(name, "back") != 0 && strcmp(name, "through") != 0) {
            fprintf(out, "错误：l1 write 只能是 back 或 through\n");
            return -1;
        }
        opt->l1_wb = strcmp(name, "back") == 0;
        return push_l1(m, opt, out);
    }
    if (starts(arg, "alloc")) {
        const char *name = rest_of(arg, "alloc");
        if (strcmp(name, "on") != 0 && strcmp(name, "off") != 0) {
            fprintf(out, "错误：l1 alloc 只能是 on 或 off\n");
            return -1;
        }
        opt->l1_alloc = strcmp(name, "on") == 0;
        return push_l1(m, opt, out);
    }
    fprintf(out, "错误：l1 命令不认识\n");
    return -1;
}

static int cmd_chip(const char *arg, FILE *out) {
    const char *p = arg;
    uint32_t units = 0;
    uint32_t word = 0;
    uint32_t chip_units = 0;
    uint32_t chip_bits = 0;
    MemChipReq req;
    if (next_num(&p, &units) != 1 || next_num(&p, &word) != 1 || next_num(&p, &chip_units) != 1 ||
        next_num(&p, &chip_bits) != 1) {
        fprintf(out, "错误：chip 单元数 字长 片单元数 片位宽\n");
        return -1;
    }
    req.units = units;
    req.word_bits = (int)word;
    req.chip_units = chip_units;
    req.chip_bits = (int)chip_bits;
    mem_chip_report(out, &req);
    return 1;
}

static int cmd_locate(const char *arg, FILE *out) {
    const char *p = arg;
    uint32_t units = 0;
    uint32_t chip_units = 0;
    uint32_t addr = 0;
    if (next_num(&p, &units) != 1 || next_num(&p, &chip_units) != 1 || next_num(&p, &addr) != 1) {
        fprintf(out, "错误：locate 单元数 片单元数 地址\n");
        return -1;
    }
    mem_chip_locate_report(out, units, chip_units, addr);
    return 1;
}

static int cmd_bank(const char *arg, FILE *out) {
    const char *p = arg;
    uint32_t modules = 0;
    uint32_t addrs[32];
    int n = 0;
    int got;
    if (next_num(&p, &modules) != 1) {
        fprintf(out, "错误：bank 模块数 地址…\n");
        return -1;
    }
    while ((got = next_num(&p, &addrs[n])) == 1) {
        n++;
        if (n >= 32) {
            break;
        }
    }
    if (got < 0 || n < 1) {
        fprintf(out, "错误：bank 模块数 地址…\n");
        return -1;
    }
    mem_bank_report(out, (int)modules, addrs, n);
    return 1;
}

static int cmd_seq(const char *arg, FILE *out) {
    const char *p = arg;
    uint32_t modules = 0;
    uint32_t access = 0;
    uint32_t bus = 0;
    uint32_t words = 0;
    if (next_num(&p, &modules) != 1 || next_num(&p, &access) != 1 || next_num(&p, &bus) != 1 ||
        next_num(&p, &words) != 1) {
        fprintf(out, "错误：seq 模块数 存取ns 总线ns 字数\n");
        return -1;
    }
    mem_seq_report(out, (int)modules, (int)access, (int)bus, (int)words);
    return 1;
}

static int read_geom(const char *arg, int *addr, int *lines, int *ways, int *block, const char **rest) {
    const char *p = arg;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    if (next_num(&p, &a) != 1 || next_num(&p, &b) != 1 || next_num(&p, &c) != 1 || next_num(&p, &d) != 1) {
        return -1;
    }
    *addr = (int)a;
    *lines = (int)b;
    *ways = (int)c;
    *block = (int)d;
    *rest = p;
    return 0;
}

static int cmd_field(const char *arg, FILE *out) {
    int addr = 0;
    int lines = 0;
    int ways = 0;
    int block = 0;
    const char *rest = arg;
    MemGeom geom;
    if (read_geom(arg, &addr, &lines, &ways, &block, &rest) != 0 ||
        mem_cache_geom(addr, lines, ways, block, 0, MEM_REPL_LRU, &geom) != 0) {
        fprintf(out, "错误：field 地址位数 行数 路数 块字节\n");
        return -1;
    }
    mem_geom_report(out, &geom, addr, block);
    return 1;
}

static int cmd_cap(const char *arg, FILE *out) {
    int addr = 0;
    int lines = 0;
    int ways = 0;
    int block = 0;
    const char *rest = arg;
    int write_back = 0;
    int replace = MEM_REPL_LRU;
    MemGeom geom;
    if (read_geom(arg, &addr, &lines, &ways, &block, &rest) != 0) {
        fprintf(out, "错误：cap 地址位数 行数 路数 块字节 back|through [lru|fifo|rand]\n");
        return -1;
    }
    while (*rest == ' ' || *rest == '\t') {
        rest++;
    }
    if (starts(rest, "back")) {
        write_back = 1;
        rest = rest_of(rest, "back");
    } else if (starts(rest, "through")) {
        write_back = 0;
        rest = rest_of(rest, "through");
    } else {
        fprintf(out, "错误：cap 要写 back 或 through\n");
        return -1;
    }
    if (strcmp(rest, "fifo") == 0) {
        replace = MEM_REPL_FIFO;
    } else if (strcmp(rest, "rand") == 0) {
        replace = MEM_REPL_RAND;
    } else if (strcmp(rest, "lru") == 0 || rest[0] == '\0') {
        replace = MEM_REPL_LRU;
    } else {
        fprintf(out, "错误：替换只能是 lru、fifo 或 rand\n");
        return -1;
    }
    if (mem_cache_geom(addr, lines, ways, block, write_back, replace, &geom) != 0) {
        fprintf(out, "错误：cap 的行数、路数、块大小要是 2 的幂\n");
        return -1;
    }
    mem_cap_report(out, &geom);
    return 1;
}

static int cmd_map(const char *arg, FILE *out) {
    int addr_bits = 0;
    int lines = 0;
    int ways = 0;
    int block = 0;
    const char *rest = arg;
    uint32_t addr = 0;
    if (read_geom(arg, &addr_bits, &lines, &ways, &block, &rest) != 0 || next_num(&rest, &addr) != 1) {
        fprintf(out, "错误：map 地址位数 行数 路数 块字节 地址\n");
        return -1;
    }
    (void)addr_bits;
    mem_map_report(out, lines, ways, block, addr);
    return 1;
}

static int cmd_walk(const char *arg, FILE *out) {
    int addr_bits = 0;
    int lines = 0;
    int ways = 0;
    int block = 0;
    const char *rest = arg;
    uint32_t base = 0;
    uint32_t rows = 0;
    uint32_t cols = 0;
    if (read_geom(arg, &addr_bits, &lines, &ways, &block, &rest) != 0 || next_num(&rest, &base) != 1 ||
        next_num(&rest, &rows) != 1 || next_num(&rest, &cols) != 1) {
        fprintf(out, "错误：walk 地址位数 行数 路数 块字节 首地址 行 列\n");
        return -1;
    }
    mem_walk_report(out, addr_bits, lines, ways, block, base, (int)rows, (int)cols);
    return 1;
}

int mm_shell_mem(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    MmMachine *m = machine == NULL ? NULL : *machine;
    if (starts(line, "l1")) {
        return cmd_l1(line, out, m, opt);
    }
    if (starts(line, "chip")) {
        return cmd_chip(rest_of(line, "chip"), out);
    }
    if (starts(line, "locate")) {
        return cmd_locate(rest_of(line, "locate"), out);
    }
    if (starts(line, "bank")) {
        return cmd_bank(rest_of(line, "bank"), out);
    }
    if (starts(line, "seq")) {
        return cmd_seq(rest_of(line, "seq"), out);
    }
    if (starts(line, "field")) {
        return cmd_field(rest_of(line, "field"), out);
    }
    if (starts(line, "cap")) {
        return cmd_cap(rest_of(line, "cap"), out);
    }
    if (starts(line, "map")) {
        return cmd_map(rest_of(line, "map"), out);
    }
    if (starts(line, "walk")) {
        return cmd_walk(rest_of(line, "walk"), out);
    }
    return 0;
}
