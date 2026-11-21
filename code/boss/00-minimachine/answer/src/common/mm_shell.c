#include "common/mm_shell.h"

#include "asm/asm_asm.h"
#include "common/mm_internal.h"
#include "common/mm_shell_stage.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_walk.h"
#include "cpu/cpu_exec.h"
#include "cpu/cpu_pipe.h"
#include "kernel/k_trap.h"
#include "prompt.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return s;
}

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

static int read_asm(FILE *in, char **src, char *err, size_t cap) {
    size_t len = 0;
    size_t room = 1024;
    char *buf = malloc(room);
    char line[256];
    if (buf == NULL) {
        mm_err(err, cap, "内存不足");
        return -1;
    }
    buf[0] = '\0';
    prompt("请输入汇编，单独一行 end 结束：");
    while (fgets(line, sizeof line, in) != NULL) {
        char *s = trim(line);
        size_t n;
        if (strcmp(s, "end") == 0) {
            break;
        }
        n = strlen(s);
        if (len + n + 2 > room) {
            char *grown = realloc(buf, room * 2);
            if (grown == NULL) {
                free(buf);
                return -1;
            }
            buf = grown;
            room *= 2;
        }
        memcpy(buf + len, s, n);
        len += n;
        buf[len++] = '\n';
        buf[len] = '\0';
        prompt("请输入汇编，单独一行 end 结束：");
    }
    *src = buf;
    return 0;
}

static void print_stop(FILE *out, const MmMachine *m, int limited) {
    int code = 0;
    if (k_exited(m, &code)) {
        fprintf(out, "停止 原因=退出 码=%d PC=%08X\n", code, m->pc);
    } else if (m->halted && m->cause >= 0) {
        fprintf(out, "停止 原因=异常 向量=%d 名称=%s PC=%08X EPC=%08X\n", m->cause, mm_cause_name(m->cause),
                m->pc, mm_sr(m, MM_SR_EPC));
    } else if (m->halted) {
        fprintf(out, "停止 原因=停机 PC=%08X\n", m->pc);
    } else if (limited) {
        fprintf(out, "停止 原因=步数上限 PC=%08X\n", m->pc);
    }
}

static MmMachine *need_m(MmMachine **slot, uint32_t bytes) {
    if (*slot == NULL) {
        *slot = mm_create(bytes);
    }
    return *slot;
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

static int cmd_set(MmMachine *m, const char *args, FILE *out) {
    char name[16];
    char value[32];
    int64_t num = 0;
    char err[64];
    int index = 0;
    if (sscanf(args, "%15s %31s", name, value) != 2 || mm_parse_i64(value, &num, err, sizeof err) != 0) {
        fprintf(out, "错误：set 需要 名字 数值\n");
        return -1;
    }
    if (strcmp(name, "pc") == 0) {
        mm_set_pc(m, (uint32_t)num);
        return 0;
    }
    if (strcmp(name, "psw") == 0) {
        mm_set_psw(m, (uint32_t)num);
        return 0;
    }
    if (!((name[0] == 'r' || name[0] == 'R') && name[1] >= '0' && name[1] <= '7' && name[2] == '\0')) {
        fprintf(out, "错误：只能设置 r0~r7、pc、psw\n");
        return -1;
    }
    index = name[1] - '0';
    mm_set_reg(m, index, (uint32_t)num);
    return 0;
}

static int one_command(char *line, FILE *in, FILE *out, MmMachine **machine, AsmImage **image,
                       uint32_t *mem_bytes, int *trace_on, MmShellOpt *opt) {
    char *s = trim(line);
    MmMachine *m;
    if (*s == '\0' || *s == '#') {
        return 0;
    }
    {
        int link = mm_shell_link(s, out, machine, *mem_bytes, opt);
        if (link != 0) {
            if (link > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return link < 0 ? -1 : 0;
        }
    }
    {
        int bus = mm_shell_bus(s, out);
        if (bus != 0) {
            return bus < 0 ? -1 : 0;
        }
    }
    {
        int io = mm_shell_io(s, out, machine, *mem_bytes, opt);
        if (io != 0) {
            if (io > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return io < 0 ? -1 : 0;
        }
    }
    {
        int fs = mm_shell_fs(s, out, machine, *mem_bytes, opt);
        if (fs != 0) {
            if (fs > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return fs < 0 ? -1 : 0;
        }
    }
    {
        int sync = mm_shell_sync(s, in, out, machine);
        if (sync != 0) {
            if (sync > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return sync < 0 ? -1 : 0;
        }
    }
    {
        int vm = mm_shell_vm(s, out, machine, opt);
        if (vm != 0) {
            if (vm > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return vm < 0 ? -1 : 0;
        }
    }
    {
        int proc = mm_shell_proc(s, in, out, machine, *mem_bytes, opt, image);
        if (proc != 0) {
            if (proc > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return proc < 0 ? -1 : 0;
        }
    }
    {
        int mmu = mm_shell_mmu(s, out, machine, *mem_bytes, opt);
        if (mmu != 0) {
            if (mmu > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return mmu < 0 ? -1 : 0;
        }
    }
    {
        int mem = mm_shell_mem(s, out, machine, opt);
        if (mem != 0) {
            if (mem > 0 && *machine != NULL) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return mem < 0 ? -1 : 0;
        }
    }
    {
        int pipe = mm_shell_pipe(s, out, machine, *mem_bytes, opt, *image);
        if (pipe != 0) {
            if (pipe > 0) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return pipe < 0 ? -1 : 0;
        }
    }
    {
        int stage = mm_shell_stage(s, out, machine, *mem_bytes, opt);
        if (stage != 0) {
            if (stage > 0) {
                mm_shell_bind(*machine, out, opt, *trace_on);
            }
            return stage < 0 ? -1 : 0;
        }
    }
    if (starts(s, "trace")) {
        const char *arg = rest_of(s, "trace");
        *trace_on = strcmp(arg, "on") == 0;
        fprintf(out, "trace %s\n", *trace_on ? "on" : "off");
        return 0;
    }
    if (starts(s, "mem")) {
        int64_t n = 0;
        char err[64];
        if (mm_parse_i64(rest_of(s, "mem"), &n, err, sizeof err) != 0 || n <= 0) {
            fprintf(out, "错误：mem 需要字节数\n");
            return -1;
        }
        mm_destroy(*machine);
        *machine = NULL;
        *mem_bytes = (uint32_t)n;
        fprintf(out, "主存 %u 字节\n", *mem_bytes);
        return 0;
    }
    if (strcmp(s, "asm") == 0) {
        char *src = NULL;
        char err[256];
        AsmImage *img = NULL;
        if (read_asm(in, &src, err, sizeof err) != 0 || asm_assemble(src, &img, err, sizeof err) != 0) {
            fprintf(out, "错误：%s\n", err[0] ? err : "汇编失败");
            free(src);
            return -1;
        }
        free(src);
        asm_free(*image);
        *image = img;
        fputs(asm_report(img), out);
        return 0;
    }
    if (strcmp(s, "load") == 0) {
        if (*image == NULL) {
            fprintf(out, "错误：还没有汇编\n");
            return -1;
        }
        m = need_m(machine, *mem_bytes);
        if (m == NULL) {
            return -1;
        }
        mm_reset(m);
        if (mm_load(m, *image) != 0) {
            fprintf(out, "错误：映像超出主存\n");
            return -1;
        }
        mm_shell_apply(m, opt);
        mm_shell_bind(m, out, opt, *trace_on);
        fprintf(out, "已装入 PC=%08X\n", mm_pc(m));
        return 0;
    }
    m = need_m(machine, *mem_bytes);
    if (m == NULL) {
        return -1;
    }
    mm_shell_bind(m, out, opt, *trace_on);
    if (starts(s, "run")) {
        const char *arg = rest_of(s, "run");
        int limit = 10000;
        int rc;
        if (*arg != '\0') {
            limit = atoi(arg);
        }
        rc = cpu_run(m, limit);
        print_stop(out, m, rc == 1);
        mm_print_stats(m, out);
        if (cpu_pipe_enabled(m)) {
            cpu_pipe_report(m, out);
        }
        if (mem_cache_on(m)) {
            mem_cache_report(m, out);
        }
        mmu_report(m, out);
        return 0;
    }
    if (strcmp(s, "step") == 0) {
        cpu_step(m);
        print_stop(out, m, 0);
        return 0;
    }
    if (strcmp(s, "regs") == 0) {
        mm_print_regs(m, out);
        return 0;
    }
    if (strcmp(s, "stats") == 0) {
        mm_print_stats(m, out);
        if (cpu_pipe_enabled(m)) {
            cpu_pipe_report(m, out);
        }
        if (mem_cache_on(m)) {
            mem_cache_report(m, out);
        }
        mmu_report(m, out);
        return 0;
    }
    if (starts(s, "dmem")) {
        char atext[32];
        char ntext[32];
        uint32_t addr = 0;
        uint32_t n = 0;
        uint32_t i;
        if (sscanf(rest_of(s, "dmem"), "%31s %31s", atext, ntext) != 2 || parse_u32(atext, &addr) != 0 ||
            parse_u32(ntext, &n) != 0) {
            fprintf(out, "错误：dmem 地址 字数\n");
            return -1;
        }
        for (i = 0; i < n; i++) {
            uint32_t word = 0;
            uint32_t at = addr + i * 4u;
            if (mm_read(m, at, 4, &word) != 0) {
                fprintf(out, "错误：读 %08X 失败\n", at);
                return -1;
            }
            fprintf(out, "%08X  %08X\n", at, word);
        }
        return 0;
    }
    if (starts(s, "set")) {
        return cmd_set(m, rest_of(s, "set"), out);
    }
    if (starts(s, "setw")) {
        char atext[32];
        char vtext[32];
        uint32_t addr = 0;
        uint32_t val = 0;
        if (sscanf(rest_of(s, "setw"), "%31s %31s", atext, vtext) != 2 || parse_u32(atext, &addr) != 0 ||
            parse_u32(vtext, &val) != 0 || mm_write(m, addr, 4, val) != 0) {
            fprintf(out, "错误：setw 地址 字\n");
            return -1;
        }
        return 0;
    }
    if (starts(s, "word")) {
        char atext[32];
        const char *arg = rest_of(s, "word");
        char *sp;
        uint32_t pc = 0;
        uint32_t words[2];
        int n = 0;
        int i;
        char err[160];
        char text[80];
        err[0] = '\0';
        if (sscanf(arg, "%31s", atext) != 1 || parse_u32(atext, &pc) != 0) {
            fprintf(out, "错误：word 地址 指令\n");
            return -1;
        }
        sp = strchr(arg, ' ');
        if (sp == NULL || asm_encode_line(trim(sp), pc, words, &n, err, sizeof err) != 0) {
            fprintf(out, "错误：%s\n", err[0] ? err : "word 地址 指令");
            return -1;
        }
        for (i = 0; i < n; i++) {
            cpu_disasm(words[i], pc + (uint32_t)i * 4u, text, sizeof text);
            fprintf(out, "%08X  %08X  %s\n", pc + (uint32_t)i * 4u, words[i], text);
        }
        return 0;
    }
    if (starts(s, "dis")) {
        char wtext[32];
        char ptext[32];
        uint32_t word = 0;
        uint32_t pc = 0;
        char text[80];
        if (sscanf(rest_of(s, "dis"), "%31s %31s", wtext, ptext) != 2 || parse_u32(wtext, &word) != 0 ||
            parse_u32(ptext, &pc) != 0 || cpu_disasm(word, pc, text, sizeof text) != 0) {
            fprintf(out, "错误：无法反汇编\n");
            return -1;
        }
        fprintf(out, "%s\n", text);
        return 0;
    }
    if (starts(s, "ea")) {
        char t0[32], t1[32], t2[32], t3[32];
        uint32_t word = 0, pc = 0, rx = 0, memv = 0;
        CpuEa ea;
        if (sscanf(rest_of(s, "ea"), "%31s %31s %31s %31s", t0, t1, t2, t3) != 4 || parse_u32(t0, &word) != 0 ||
            parse_u32(t1, &pc) != 0 || parse_u32(t2, &rx) != 0 || parse_u32(t3, &memv) != 0 ||
            cpu_ea(word, pc, rx, memv, &ea) != 0) {
            fprintf(out, "错误：ea 需要 机器码 PC 寄存器值 间接字\n");
            return -1;
        }
        if (ea.has_ea) {
            fprintf(out, "方式=%d EA=%08X 取操作数访存=%d\n", ea.mode, ea.ea, ea.mem_ops);
        } else {
            fprintf(out, "方式=%d EA=无 取操作数访存=%d\n", ea.mode, ea.mem_ops);
        }
        return 0;
    }
    fprintf(out, "错误：未知命令 %s\n", s);
    return -1;
}

int mm_shell(FILE *in, FILE *out) {
    char line[256];
    MmMachine *machine = NULL;
    AsmImage *image = NULL;
    MmShellOpt opt;
    uint32_t mem_bytes = MM_MEM_MAX;
    int trace_on = 0;
    int rc = 0;
    mm_shell_opt_init(&opt);
    while (rc == 0 && !feof(in)) {
        prompt("请输入命令：");
        if (fgets(line, sizeof line, in) == NULL) {
            break;
        }
        if (one_command(line, in, out, &machine, &image, &mem_bytes, &trace_on, &opt) != 0) {
            rc = 1;
        }
    }
    asm_free(image);
    mm_destroy(machine);
    return rc;
}
