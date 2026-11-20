#include "common/mm_shell_stage.h"

#include "asm/asm_asm.h"
#include "common/mm_internal.h"
#include "cpu/cpu_exec.h"
#include "cpu/cpu_pipe.h"
#include "kernel/k_trap.h"

#include <stdlib.h>
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
    if (mm_parse_i64(text, &num, err, sizeof err) != 0 || num <= 0 || num > 1000000) {
        return -1;
    }
    *out = (uint32_t)num;
    return 0;
}

static void push(MmMachine *m, const MmShellOpt *opt) {
    CpuPipeOpt pipe;
    if (m == NULL) {
        return;
    }
    cpu_pipe_opt_default(&pipe);
    pipe.forward = opt->forward;
    pipe.branch_stall = opt->branch_stall;
    pipe.unified = opt->unified;
    pipe.diagram = opt->diagram;
    pipe.freq_mhz = opt->freq_mhz;
    cpu_pipe_set(m, &pipe, opt->pipe);
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

static int capture(const MmMachine *m, uint8_t **mem, uint32_t regs[8], uint32_t *pc, uint32_t *psw, int *cause,
                   char *con, size_t cap) {
    int i;
    *mem = malloc(m->mem_size);
    if (*mem == NULL) {
        return -1;
    }
    memcpy(*mem, m->mem, m->mem_size);
    for (i = 0; i < 8; i++) {
        regs[i] = m->r[i];
    }
    *pc = m->pc;
    *psw = m->psw & 0xffu;
    *cause = m->cause;
    snprintf(con, cap, "%s", m->con_out);
    return 0;
}

static int same_state(const MmMachine *m, const uint8_t *mem, const uint32_t regs[8], uint32_t pc, uint32_t psw,
                      int cause, const char *con) {
    int i;
    if (memcmp(m->mem, mem, m->mem_size) != 0 || m->pc != pc || (m->psw & 0xffu) != psw || m->cause != cause) {
        return 0;
    }
    if (strcmp(m->con_out, con) != 0) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        if (m->r[i] != regs[i]) {
            return 0;
        }
    }
    return 1;
}

static int run_quiet(MmMachine *m, const AsmImage *image, const MmShellOpt *opt, int pipe) {
    mm_reset(m);
    if (mm_load(m, image) != 0) {
        return -1;
    }
    mm_shell_apply(m, opt);
    m->pipe_on = pipe;
    mm_set_trace(m, NULL);
    cpu_set_event(m, NULL);
    cpu_run(m, 100000);
    return (m->halted || k_exited(m, NULL)) ? 0 : -1;
}

static int cmd_compare(MmMachine **slot, uint32_t bytes, const MmShellOpt *opt, AsmImage *image, FILE *out) {
    uint8_t *mem = NULL;
    uint32_t regs[8];
    uint32_t pc = 0;
    uint32_t psw = 0;
    uint32_t tick_cycles;
    uint32_t tick_insns;
    int cause = 0;
    char con[512];
    int same;
    if (image == NULL) {
        fprintf(out, "错误：还没有汇编\n");
        return -1;
    }
    if (*slot == NULL) {
        *slot = mm_create(bytes);
    }
    if (*slot == NULL || run_quiet(*slot, image, opt, 0) != 0) {
        fprintf(out, "错误：节拍模式没有停机\n");
        return -1;
    }
    tick_cycles = mm_cycles(*slot);
    tick_insns = mm_insn_count(*slot);
    if (capture(*slot, &mem, regs, &pc, &psw, &cause, con, sizeof con) != 0) {
        return -1;
    }
    if (run_quiet(*slot, image, opt, 1) != 0) {
        fprintf(out, "错误：流水线没有停机\n");
        free(mem);
        return -1;
    }
    same = same_state(*slot, mem, regs, pc, psw, cause, con);
    cpu_pipe_compare(out, tick_cycles, tick_insns, *slot, same);
    free(mem);
    return 1;
}

int mm_shell_pipe(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt, AsmImage *image) {
    int rc;
    if (starts(line, "mode")) {
        const char *arg = rest_of(line, "mode");
        if (strcmp(arg, "pipe") != 0 && strcmp(arg, "tick") != 0) {
            fprintf(out, "错误：mode 只能是 tick 或 pipe\n");
            return -1;
        }
        opt->pipe = strcmp(arg, "pipe") == 0;
        fprintf(out, "模式 %s\n", opt->pipe ? "流水线" : "节拍");
        push(*machine, opt);
        return 1;
    }
    if (starts(line, "forward")) {
        rc = on_off(rest_of(line, "forward"), &opt->forward, "转发", out);
        if (rc > 0) {
            push(*machine, opt);
        }
        return rc;
    }
    if (starts(line, "branch")) {
        const char *arg = rest_of(line, "branch");
        if (strcmp(arg, "predict") != 0 && strcmp(arg, "stall") != 0) {
            fprintf(out, "错误：branch 只能是 predict 或 stall\n");
            return -1;
        }
        opt->branch_stall = strcmp(arg, "stall") == 0;
        fprintf(out, "分支 %s\n", opt->branch_stall ? "阻塞到MEM" : "预测不转移");
        push(*machine, opt);
        return 1;
    }
    if (starts(line, "cache")) {
        const char *arg = rest_of(line, "cache");
        if (strcmp(arg, "split") != 0 && strcmp(arg, "unified") != 0) {
            fprintf(out, "错误：cache 只能是 split 或 unified\n");
            return -1;
        }
        opt->unified = strcmp(arg, "unified") == 0;
        fprintf(out, "存储器 %s\n", opt->unified ? "统一" : "分开");
        push(*machine, opt);
        return 1;
    }
    if (starts(line, "freq")) {
        if (parse_u32(rest_of(line, "freq"), &opt->freq_mhz) != 0) {
            fprintf(out, "错误：freq 需要主频，单位 MHz\n");
            return -1;
        }
        fprintf(out, "主频 %u MHz\n", opt->freq_mhz);
        push(*machine, opt);
        return 1;
    }
    if (starts(line, "diagram")) {
        rc = on_off(rest_of(line, "diagram"), &opt->diagram, "时空图", out);
        if (rc > 0) {
            push(*machine, opt);
        }
        return rc;
    }
    if (strcmp(line, "compare") == 0) {
        return cmd_compare(machine, mem_bytes, opt, image, out);
    }
    return 0;
}
