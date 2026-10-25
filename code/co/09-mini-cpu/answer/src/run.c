#include "run.h"

#include "beat.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    FILE *out;
    int index;
    Cpu *cpu;
} Walk;

static void step(Walk *w, const char *stage, const char *func, const char *detail, const char *sig) {
    Beat beat;
    w->index++;
    beat.index = w->index;
    beat.stage = stage;
    beat.func = func;
    beat.detail = detail;
    beat.sig = sig;
    w->cpu->conflicts += beat_print(w->out, &beat);
}

static void fetch(Walk *w) {
    Cpu *c = w->cpu;
    char detail[64];
    char inst[32];
    c->mar = c->pc;
    snprintf(detail, sizeof detail, "PC=%u MAR=%u", c->pc, c->mar);
    step(w, "取指", "(PC)->MAR", detail, "PCout MARin");
    c->mdr = c->mem[c->mar % MEM_WORDS];
    c->visits++;
    c->pc = (uint16_t)(c->pc + 1);
    snprintf(detail, sizeof detail, "MDR=%04XH PC=%u", c->mdr, c->pc);
    step(w, "取指", "M(MAR)->MDR (PC)+1->PC", detail, "MemR MDRinE PC+1");
    c->ir = c->mdr;
    fmt_inst(inst, sizeof inst, c->ir);
    snprintf(detail, sizeof detail, "IR=%s", inst);
    step(w, "取指", "(MDR)->IR", detail, "MDRout IRin");
    step(w, "译码", "OP(IR)->CU", detail, "无");
}

static void exec_mem_addr(Walk *w, uint16_t addr) {
    char detail[32];
    w->cpu->mar = addr;
    snprintf(detail, sizeof detail, "MAR=%u", addr);
    step(w, "执行", "(IR的地址)->MAR", detail, "IRout MARin");
}

static void exec_lda(Walk *w) {
    Cpu *c = w->cpu;
    char detail[48];
    exec_mem_addr(w, (uint16_t)(c->ir & 0xFF));
    c->mdr = c->mem[c->mar];
    c->visits++;
    snprintf(detail, sizeof detail, "MDR=%u(%04XH)", c->mdr, c->mdr);
    step(w, "访存", "M(MAR)->MDR", detail, "MemR MDRinE");
    c->acc = c->mdr;
    step(w, "写回", "(MDR)->ACC", detail, "MDRout ACCin");
}

static void exec_sta(Walk *w) {
    Cpu *c = w->cpu;
    char detail[48];
    exec_mem_addr(w, (uint16_t)(c->ir & 0xFF));
    c->mdr = c->acc;
    snprintf(detail, sizeof detail, "MDR=%u(%04XH)", c->mdr, c->mdr);
    step(w, "访存", "(ACC)->MDR", detail, "ACCout MDRin");
    c->mem[c->mar] = c->mdr;
    c->dirty[c->mar] = 1;
    c->visits++;
    step(w, "访存", "(MDR)->M(MAR)", "写主存，走数据总线", "MDRoutE MemW");
}

static void exec_add(Walk *w) {
    Cpu *c = w->cpu;
    char detail[80];
    uint16_t mem;
    uint16_t sum;
    exec_mem_addr(w, (uint16_t)(c->ir & 0xFF));
    c->mdr = c->mem[c->mar];
    c->visits++;
    mem = c->mdr;
    snprintf(detail, sizeof detail, "MDR=%u(%04XH)", mem, mem);
    step(w, "访存", "M(MAR)->MDR", detail, "MemR MDRinE");
    c->a = mem;
    step(w, "执行", "(MDR)->A", detail, "MDRout Ain");
    psw_add(c->a, c->acc, &sum, &c->psw);
    snprintf(detail, sizeof detail, "A=%u ACC旧=%u 和=%u(%04XH)", c->a, c->acc, sum, sum);
    c->acc = sum;
    step(w, "写回", "(A)+(ACC)->ACC", detail, "ACCout Add ACin");
}

static void exec_jump(Walk *w, int taken) {
    Cpu *c = w->cpu;
    char detail[48];
    uint16_t addr = (uint16_t)(c->ir & 0xFF);
    if (!taken) {
        step(w, "执行", "ZF=0 不转移", "PC 保持取指之后的值", "无");
        return;
    }
    c->pc = addr;
    snprintf(detail, sizeof detail, "PC=%u", addr);
    step(w, "执行", "(IR的地址)->PC", detail, "IRout PCin");
}

static int exec_adm(Walk *w) {
    Cpu *c = w->cpu;
    char detail[80];
    int reg = (c->ir >> 8) & 3;
    uint16_t addr = c->r[reg];
    uint16_t sum;
    if (addr >= MEM_WORDS) {
        fprintf(stderr, "ADM 的地址 %u 超出主存\n", addr);
        return -1;
    }
    c->mar = addr;
    snprintf(detail, sizeof detail, "R%d=%u MAR=%u", reg, addr, addr);
    step(w, "访存", "(R)->MAR", detail, "Rout MARin");
    c->mdr = c->mem[addr];
    c->visits++;
    snprintf(detail, sizeof detail, "MDR=%u(%04XH)", c->mdr, c->mdr);
    step(w, "访存", "M(MAR)->MDR", detail, "MemR MDRinE");
    c->a = c->mdr;
    step(w, "执行", "(MDR)->A", detail, "MDRout Ain");
    psw_add(c->a, c->r[0], &sum, &c->psw);
    snprintf(detail, sizeof detail, "A=%u R0=%u 和=%u(%04XH)", c->a, c->r[0], sum, sum);
    c->acc = sum;
    step(w, "执行", "(A)+(R0)->ACC", detail, "R0out Add ACin");
    c->mdr = c->acc;
    step(w, "写回", "(ACC)->MDR", "MAR 仍是刚才的地址", "ACCout MDRin");
    c->mem[addr] = c->mdr;
    c->dirty[addr] = 1;
    c->visits++;
    snprintf(detail, sizeof detail, "M[%u]=%u(%04XH)", addr, c->mdr, c->mdr);
    step(w, "访存", "(MDR)->M(MAR)", detail, "MDRoutE MemW");
    return 0;
}

static int execute(Walk *w) {
    switch (w->cpu->ir >> 12) {
    case OP_NOP:
        step(w, "执行", "空操作", "取指已经把 PC 加 1", "无");
        return 0;
    case OP_LDA:
        exec_lda(w);
        return 0;
    case OP_STA:
        exec_sta(w);
        return 0;
    case OP_ADD:
        exec_add(w);
        return 0;
    case OP_JMP:
        exec_jump(w, 1);
        return 0;
    case OP_JZ:
        exec_jump(w, w->cpu->psw.zf);
        return 0;
    case OP_ADM:
        return exec_adm(w);
    case OP_HLT:
        w->cpu->halted = 1;
        step(w, "执行", "停机", "不再取下一条", "无");
        return 0;
    default:
        fprintf(stderr, "无法执行的操作码\n");
        return -1;
    }
}

static void show_state(const Walk *w, int visits_before) {
    const Cpu *c = w->cpu;
    fprintf(w->out,
            "状态 PC=%u ACC=%u(%04XH) R0=%u R1=%u R2=%u R3=%u ZF=%d SF=%d CF=%d OF=%d 本条访存=%d次\n",
            c->pc, c->acc, c->acc, c->r[0], c->r[1], c->r[2], c->r[3], c->psw.zf, c->psw.sf, c->psw.cf,
            c->psw.of, c->visits - visits_before);
}

static void show_load(FILE *out, const Cpu *cpu, int ncode) {
    int i;
    char text[32];
    char hex[16];
    fprintf(out, "模型：16 位字，按字编址，主存 %d 字。取指 (PC)+1。单总线同一拍至多 1 个 out\n", MEM_WORDS);
    fprintf(out, "【装入】\n");
    for (i = 0; i < ncode; i++) {
        fmt_inst(text, sizeof text, cpu->mem[i]);
        fmt_hex(hex, sizeof hex, cpu->mem[i]);
        fprintf(out, "M[%d] %s 编码 %s\n", i, text, hex);
    }
    for (i = 0; i < 4; i++) {
        if (cpu->r[i] != 0) {
            fprintf(out, "R%d=%u(%04XH)\n", i, cpu->r[i], cpu->r[i]);
        }
    }
    for (i = ncode; i < MEM_WORDS; i++) {
        if (cpu->mem[i] != 0) {
            fprintf(out, "初值 M[%d]=%u(%04XH)\n", i, cpu->mem[i], cpu->mem[i]);
        }
    }
}

static void show_done(FILE *out, const Cpu *cpu) {
    int i;
    int any = 0;
    fprintf(out, "【写回的单元】\n");
    for (i = 0; i < MEM_WORDS; i++) {
        if (cpu->dirty[i]) {
            fprintf(out, "M[%d]=%u(%04XH)\n", i, cpu->mem[i], cpu->mem[i]);
            any = 1;
        }
    }
    if (!any) {
        fprintf(out, "无\n");
    }
    fprintf(out, "结果：ACC=%u(%04XH) 访存=%d次 冲突=%d次\n", cpu->acc, cpu->acc, cpu->visits, cpu->conflicts);
}

static int load_image(const Program *prog, Cpu *cpu) {
    int i;
    memset(cpu, 0, sizeof *cpu);
    if (prog->ncode < 1 || prog->ncode > MAX_CODE) {
        fprintf(stderr, "指令条数要在 1 到 %d 之间\n", MAX_CODE);
        return -1;
    }
    for (i = 0; i < prog->ncode; i++) {
        cpu->mem[i] = prog->code[i];
    }
    for (i = 0; i < 4; i++) {
        if (prog->reg_mask & (1 << i)) {
            cpu->r[i] = prog->reg[i];
        }
    }
    for (i = 0; i < prog->ndata; i++) {
        cpu->mem[prog->daddr[i]] = prog->dval[i];
    }
    return 0;
}

int cpu_exec(const Program *prog, Cpu *cpu, FILE *out) {
    Walk w;
    if (load_image(prog, cpu) != 0) {
        return -1;
    }
    show_load(out, cpu, prog->ncode);
    fprintf(out, "【执行】\n");
    w.out = out;
    w.cpu = cpu;
    while (!cpu->halted && cpu->insns < MAX_STEPS) {
        int before = cpu->visits;
        w.index = 0;
        fetch(&w);
        if (execute(&w) != 0) {
            return -1;
        }
        cpu->insns++;
        show_state(&w, before);
    }
    if (!cpu->halted) {
        fprintf(out, "超过 %d 条指令，停止\n", MAX_STEPS);
    }
    show_done(out, cpu);
    return 0;
}
