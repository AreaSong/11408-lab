#include "common/mm_shell_stage.h"

#include "common/mm_internal.h"
#include "cpu/cpu_intr.h"
#include "cpu/cpu_pipe.h"
#include "kernel/k_trap.h"
#include "mem/mem_cache.h"

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

void mm_shell_opt_init(MmShellOpt *opt) {
    memset(opt, 0, sizeof *opt);
    opt->forward = 1;
    opt->l1_lines = 8;
    opt->l1_ways = 1;
    opt->l1_block = 16;
    opt->l1_replace = MEM_REPL_LRU;
    opt->l1_wb = 1;
    opt->l1_alloc = 1;
    opt->l1_mem_cycles = 4;
    opt->l1_split = 1;
    opt->l1_seed = 1;
}

void mm_shell_apply(MmMachine *m, const MmShellOpt *opt) {
    int i;
    uint32_t psw;
    if (opt->kernel) {
        k_boot(m);
    }
    psw = mm_psw(m);
    if (opt->user) {
        psw |= MM_PSW_U;
    }
    if (opt->ei) {
        psw |= MM_PSW_IF;
    }
    mm_set_psw(m, psw);
    if (opt->imr_set) {
        cpu_set_imr(m, opt->imr);
    }
    for (i = 0; i < 16; i++) {
        if (opt->mask_set[i]) {
            k_set_irq_mask(m, i, opt->mask[i]);
        }
        if (opt->steps_set[i]) {
            k_set_irq_steps(m, i, opt->steps[i]);
        }
        if (opt->raise[i]) {
            cpu_raise_irq(m, i);
        }
    }
    if (opt->nmi) {
        cpu_raise_nmi(m);
    }
    if (opt->conin[0] != '\0') {
        k_console_set_in(m, opt->conin);
    }
    if (opt->timer_set) {
        m->port[0x11] = opt->timer;
        m->port[0x10] = opt->timer == 0 ? 0 : 1;
    }
    {
        CpuPipeOpt pipe;
        cpu_pipe_opt_default(&pipe);
        pipe.forward = opt->forward;
        pipe.branch_stall = opt->branch_stall;
        pipe.unified = opt->unified;
        pipe.diagram = opt->diagram;
        pipe.freq_mhz = opt->freq_mhz;
        cpu_pipe_set(m, &pipe, opt->pipe);
    }
    mm_shell_mem_apply(m, opt);
}

void mm_shell_bind(MmMachine *m, FILE *out, const MmShellOpt *opt, int trace_on) {
    if (m == NULL) {
        return;
    }
    mm_set_trace(m, trace_on ? out : NULL);
    cpu_set_event(m, opt->event ? out : NULL);
}

static int on_off(const char *arg, int *flag, const char *name, FILE *out) {
    if (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0) {
        fprintf(out, "错误：%s 只能是 on 或 off\n", name);
        return -1;
    }
    *flag = strcmp(arg, "on") == 0;
    fprintf(out, "%s %s\n", name, *flag ? "on" : "off");
    return 1;
}

static int cmd_mask(MmShellOpt *opt, const char *arg, FILE *out) {
    char a[32];
    char b[32];
    uint32_t irq = 0;
    uint32_t mask = 0;
    if (sscanf(arg, "%31s %31s", a, b) != 2 || parse_u32(a, &irq) != 0 || parse_u32(b, &mask) != 0 ||
        irq >= 16) {
        fprintf(out, "错误：mask IRQ号 屏蔽字\n");
        return -1;
    }
    opt->mask_set[irq] = 1;
    opt->mask[irq] = mask & 0xffffu;
    fprintf(out, "屏蔽字 IRQ%u=%04X\n", irq, opt->mask[irq]);
    return 1;
}

static int cmd_service(MmShellOpt *opt, const char *arg, FILE *out) {
    char a[32];
    char b[32];
    uint32_t irq = 0;
    uint32_t steps = 0;
    if (sscanf(arg, "%31s %31s", a, b) != 2 || parse_u32(a, &irq) != 0 || parse_u32(b, &steps) != 0 ||
        irq >= 16 || steps == 0 || steps > 32) {
        fprintf(out, "错误：service IRQ号 步数\n");
        return -1;
    }
    opt->steps_set[irq] = 1;
    opt->steps[irq] = (int)steps;
    fprintf(out, "服务 IRQ%u 步数=%u\n", irq, steps);
    return 1;
}

static int cmd_raise(MmShellOpt *opt, MmMachine *m, const char *arg, FILE *out) {
    uint32_t irq = 0;
    if (strcmp(arg, "nmi") == 0) {
        opt->nmi = 1;
        if (m != NULL) {
            cpu_raise_nmi(m);
        }
        fprintf(out, "请求 NMI\n");
        return 1;
    }
    if (parse_u32(arg, &irq) != 0 || irq >= 16) {
        fprintf(out, "错误：raise IRQ号 或 raise nmi\n");
        return -1;
    }
    opt->raise[irq] = 1;
    if (m != NULL) {
        cpu_raise_irq(m, (int)irq);
    }
    fprintf(out, "请求 IRQ%u\n", irq);
    return 1;
}

static int cmd_sr(MmMachine *m, const char *arg, FILE *out) {
    static const char *name[] = {"epc", "epsw", "cause", "badva", "ptbr", "imr", "ksp", "vbr", "psw"};
    int i;
    if (m == NULL) {
        fprintf(out, "错误：还没有机器\n");
        return -1;
    }
    for (i = 0; i < 9; i++) {
        if (strcmp(arg, name[i]) == 0) {
            fprintf(out, "%s=%08X\n", name[i], mm_sr(m, i));
            return 1;
        }
    }
    fprintf(out, "错误：sr 只能是 epc/epsw/cause/badva/ptbr/imr/ksp/vbr/psw\n");
    return -1;
}

static int cmd_conout(MmMachine *m, FILE *out) {
    char buf[512];
    if (m == NULL) {
        fprintf(out, "错误：还没有机器\n");
        return -1;
    }
    k_console_out(m, buf, (int)sizeof buf);
    fprintf(out, "控制台 %s\n", buf);
    return 1;
}

static MmMachine *need_machine(MmMachine **slot, uint32_t bytes) {
    if (*slot == NULL) {
        *slot = mm_create(bytes);
    }
    return *slot;
}

static int stage_mode(char *s, FILE *out, MmShellOpt *opt) {
    if (starts(s, "kernel")) {
        return on_off(rest_of(s, "kernel"), &opt->kernel, "kernel", out);
    }
    if (starts(s, "user")) {
        return on_off(rest_of(s, "user"), &opt->user, "user", out);
    }
    if (starts(s, "if")) {
        return on_off(rest_of(s, "if"), &opt->ei, "if", out);
    }
    if (starts(s, "event")) {
        return on_off(rest_of(s, "event"), &opt->event, "event", out);
    }
    return 0;
}

static int stage_device(char *s, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt) {
    if (starts(s, "mask")) {
        return cmd_mask(opt, rest_of(s, "mask"), out);
    }
    if (starts(s, "service")) {
        return cmd_service(opt, rest_of(s, "service"), out);
    }
    if (starts(s, "raise")) {
        return cmd_raise(opt, *machine, rest_of(s, "raise"), out);
    }
    if (starts(s, "imr")) {
        if (parse_u32(rest_of(s, "imr"), &opt->imr) != 0) {
            fprintf(out, "错误：imr 需要数值\n");
            return -1;
        }
        opt->imr_set = 1;
        fprintf(out, "IMR=%04X\n", opt->imr & 0xffffu);
        return 1;
    }
    if (starts(s, "timer")) {
        const char *arg = rest_of(s, "timer");
        if (strcmp(arg, "off") == 0) {
            opt->timer_set = 1;
            opt->timer = 0;
            fprintf(out, "timer off\n");
            return 1;
        }
        if (parse_u32(arg, &opt->timer) != 0) {
            fprintf(out, "错误：timer 周期 或 timer off\n");
            return -1;
        }
        opt->timer_set = 1;
        fprintf(out, "timer %u\n", opt->timer);
        return 1;
    }
    if (starts(s, "conin")) {
        snprintf(opt->conin, sizeof opt->conin, "%s", rest_of(s, "conin"));
        fprintf(out, "键盘 %s\n", opt->conin);
        return 1;
    }
    if (strcmp(s, "conout") == 0) {
        return cmd_conout(*machine, out);
    }
    if (starts(s, "sr")) {
        return cmd_sr(need_machine(machine, mem_bytes), rest_of(s, "sr"), out);
    }
    return 0;
}

int mm_shell_stage(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt) {
    int mode = stage_mode(line, out, opt);
    if (mode != 0) {
        return mode;
    }
    return stage_device(line, out, machine, mem_bytes, opt);
}
