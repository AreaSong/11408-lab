#include "sim.h"

#include "wire.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t mem[MEM_WORDS];
    uint16_t pc, ir, acc, mar, mdr, saved;
    int ie, kernel, halted, bad, took;
    int irq_at, nmi, irq_pending;
    int entry;
    FILE *out;
} Mach;

typedef struct {
    int at, op, arg;
} SIns;

typedef struct {
    int at, val;
} SData;

typedef struct {
    const char *name;
    const char *title;
    int entry, ie, irq_at, nmi, nins, ndata;
    SIns ins[12];
    SData data[6];
} Scene;

static void trace(Mach *m, int beat, int op, int zero, const char *detail) {
    char sig[160];
    uint32_t bits = wire_sig(op, beat, zero);
    const char *stage = beat <= 2 ? "取指" : (beat == 3 ? "译码" : "执行");
    format_sigs(sig, sizeof sig, bits);
    fprintf(m->out, "T%d %s %s | %s | 内总线驱动 %d 个\n",
            beat, stage, detail, sig, out_count(bits));
}

static int bad_addr(Mach *m, int addr) {
    fprintf(stderr, "地址 %d 超出主存\n", addr);
    m->bad = 1;
    return -1;
}

static void finish(Mach *m) {
    if (m->irq_pending && (m->nmi || m->ie)) {
        const char *kind = m->nmi ? "不可屏蔽" : "可屏蔽";
        m->saved = m->pc;
        m->ie = 0;
        m->kernel = 1;
        m->took++;
        fprintf(m->out, "指令结束 响应%s中断 | 断点 %u | 关中断 IF=0 内核态 | 入口 %d -> PC\n",
                kind, m->saved, m->entry);
        m->pc = (uint16_t)m->entry;
        m->irq_pending = 0;
        m->nmi = 0;
        return;
    }
    if (m->irq_pending) {
        fprintf(m->out, "指令结束 下一条字地址 %u | 可屏蔽中断被 IF=0 挡住\n", m->pc);
        return;
    }
    fprintf(m->out, "指令结束 下一条字地址 %u | 外部中断 无\n", m->pc);
}

static void raise_fault(Mach *m, uint16_t insn_pc) {
    uint16_t bumped = m->pc;
    m->saved = insn_pc;
    m->ie = 0;
    m->kernel = 1;
    m->took++;
    fprintf(m->out,
            "异常 故障 除零 | 响应在本条指令执行过程中 | 断点 %u（取指后 PC 已是 %u） | IF=0 内核态 | 入口 %d -> PC\n",
            insn_pc, bumped, m->entry);
    m->pc = (uint16_t)m->entry;
}

static void raise_trap(Mach *m) {
    m->saved = m->pc;
    m->ie = 0;
    m->kernel = 1;
    m->took++;
    trace(m, 4, OP_TRAP, 0, "系统调用");
    fprintf(m->out, "异常 陷阱 系统调用 | 本条指令主动陷入 | 断点 %u 是下一条 | IF=0 内核态 | 入口 %d -> PC\n",
            m->saved, m->entry);
    m->pc = (uint16_t)m->entry;
}

static void raise_abort(Mach *m, uint16_t insn_pc) {
    m->pc = insn_pc;
    m->ie = 0;
    m->kernel = 1;
    m->halted = 1;
    m->took++;
    fprintf(m->out, "异常 终止 硬件故障 | 响应在译码时 | 不保存返回断点 | 不转到入口 | PC=%u IF=0 内核态\n",
            insn_pc);
}

static void fetch(Mach *m, uint16_t *insn_pc) {
    char detail[80];
    char text[32];
    *insn_pc = m->pc;
    fprintf(m->out, "----\n");
    if (m->irq_at >= 0 && (int)m->pc == m->irq_at) {
        m->irq_pending = 1;
        fprintf(m->out, "中断请求到达 | 字地址 %u 执行期间 | %s\n",
                m->pc, m->nmi ? "不可屏蔽" : "可屏蔽");
        m->irq_at = -1;
    }
    m->mar = m->pc;
    snprintf(detail, sizeof detail, "PC=%u -> MAR", m->pc);
    trace(m, 0, OP_NOP, 0, detail);
    m->mdr = m->mem[m->mar];
    m->pc = (uint16_t)(m->pc + 1);
    snprintf(detail, sizeof detail, "MDR=%04XH PC=%u", m->mdr, m->pc);
    trace(m, 1, OP_NOP, 0, detail);
    m->ir = m->mdr;
    format_inst(text, sizeof text, m->ir);
    snprintf(detail, sizeof detail, "IR=%s", text);
    trace(m, 2, OP_NOP, 0, detail);
    trace(m, 3, OP_NOP, 0, "操作码送控制单元");
}

static int do_lda(Mach *m, int addr) {
    char detail[80];
    if (addr < 0 || addr >= MEM_WORDS) return bad_addr(m, addr);
    m->mar = (uint16_t)addr;
    snprintf(detail, sizeof detail, "MAR=%d", addr);
    trace(m, 4, OP_LDA, 0, detail);
    m->mdr = m->mem[addr];
    snprintf(detail, sizeof detail, "MDR=%u", m->mdr);
    trace(m, 5, OP_LDA, 0, detail);
    m->acc = m->mdr;
    snprintf(detail, sizeof detail, "MDR -> ACC=%u", m->acc);
    trace(m, 6, OP_LDA, 0, detail);
    finish(m);
    return 0;
}

static int do_sta(Mach *m, int addr) {
    char detail[80];
    if (addr < 0 || addr >= MEM_WORDS) return bad_addr(m, addr);
    m->mar = (uint16_t)addr;
    snprintf(detail, sizeof detail, "MAR=%d", addr);
    trace(m, 4, OP_STA, 0, detail);
    m->mdr = m->acc;
    snprintf(detail, sizeof detail, "ACC=%u -> MDR", m->acc);
    trace(m, 5, OP_STA, 0, detail);
    m->mem[addr] = m->mdr;
    snprintf(detail, sizeof detail, "M[%d]=%u", addr, m->mdr);
    trace(m, 6, OP_STA, 0, detail);
    finish(m);
    return 0;
}

static int do_add(Mach *m, int addr) {
    char detail[80];
    uint16_t sum;
    if (addr < 0 || addr >= MEM_WORDS) return bad_addr(m, addr);
    m->mar = (uint16_t)addr;
    snprintf(detail, sizeof detail, "MAR=%d", addr);
    trace(m, 4, OP_ADD, 0, detail);
    m->mdr = m->mem[addr];
    snprintf(detail, sizeof detail, "MDR=%u", m->mdr);
    trace(m, 5, OP_ADD, 0, detail);
    snprintf(detail, sizeof detail, "MDR 送入暂存器 A");
    trace(m, 6, OP_ADD, 0, detail);
    sum = (uint16_t)(m->acc + m->mdr);
    snprintf(detail, sizeof detail, "ACC=%u+%u=%u", m->acc, m->mdr, sum);
    m->acc = sum;
    trace(m, 7, OP_ADD, 0, detail);
    finish(m);
    return 0;
}

static int do_div(Mach *m, uint16_t insn_pc, int addr) {
    char detail[96];
    if (addr < 0 || addr >= MEM_WORDS) return bad_addr(m, addr);
    m->mar = (uint16_t)addr;
    snprintf(detail, sizeof detail, "MAR=%d", addr);
    trace(m, 4, OP_DIV, 0, detail);
    m->mdr = m->mem[addr];
    snprintf(detail, sizeof detail, "MDR=%u", m->mdr);
    trace(m, 5, OP_DIV, 0, detail);
    if (m->mdr == 0) {
        trace(m, 6, OP_DIV, 1, "除数为 0");
        raise_fault(m, insn_pc);
        return 0;
    }
    snprintf(detail, sizeof detail, "除数 %u 送入 A", m->mdr);
    trace(m, 6, OP_DIV, 0, detail);
    {
        uint16_t q = (uint16_t)(m->acc / m->mdr);
        snprintf(detail, sizeof detail, "商 %u/%u=%u -> ACC", m->acc, m->mdr, q);
        m->acc = q;
    }
    trace(m, 7, OP_DIV, 0, detail);
    finish(m);
    return 0;
}

static int do_jmp(Mach *m, int addr) {
    char detail[64];
    if (addr < 0 || addr >= MEM_WORDS) return bad_addr(m, addr);
    m->pc = (uint16_t)addr;
    snprintf(detail, sizeof detail, "PC=%d", addr);
    trace(m, 4, OP_JMP, 0, detail);
    finish(m);
    return 0;
}

static void do_iret(Mach *m) {
    char detail[80];
    uint16_t back = m->saved;
    m->pc = back;
    m->ie = 1;
    m->kernel = 0;
    snprintf(detail, sizeof detail, "断点 %u -> PC，IF=1，用户态", back);
    trace(m, 4, OP_IRET, 0, detail);
    finish(m);
}

static int step(Mach *m) {
    uint16_t insn_pc;
    int op;
    int arg;
    if (m->pc >= MEM_WORDS) return bad_addr(m, (int)m->pc);
    fetch(m, &insn_pc);
    op = (int)(m->ir >> 12);
    arg = (int)(m->ir & 0xFF);
    switch (op) {
    case OP_NOP:
        trace(m, 4, OP_NOP, 0, "空操作");
        finish(m);
        return 0;
    case OP_LDA: return do_lda(m, arg);
    case OP_STA: return do_sta(m, arg);
    case OP_ADD: return do_add(m, arg);
    case OP_DIV: return do_div(m, insn_pc, arg);
    case OP_JMP: return do_jmp(m, arg);
    case OP_TRAP: raise_trap(m); return 0;
    case OP_HLT:
        trace(m, 4, OP_HLT, 0, "停机");
        fprintf(m->out, "停机\n");
        m->halted = 1;
        return 0;
    case OP_IRET: do_iret(m); return 0;
    case OP_FAIL: raise_abort(m, insn_pc); return 0;
    default:
        raise_abort(m, insn_pc);
        return 0;
    }
}

static int run_mach(Mach *m) {
    int steps = 0;
    while (!m->halted && !m->bad && steps < 40) {
        if (step(m) != 0) return -1;
        steps++;
    }
    if (!m->halted && !m->bad) {
        fprintf(m->out, "步数超过 40 条\n");
        return -1;
    }
    return m->bad ? -1 : 0;
}

static void fill_result(const Mach *m, SceneResult *r) {
    int i;
    if (r == NULL) return;
    r->acc = m->acc;
    r->pc = m->pc;
    r->saved = m->saved;
    r->took = m->took;
    r->ie = m->ie;
    r->halted = m->halted;
    for (i = 0; i < MEM_WORDS; i++) r->mem[i] = m->mem[i];
}

static void load_scene(Mach *m, const Scene *scene, FILE *out) {
    int i;
    memset(m, 0, sizeof *m);
    m->out = out;
    m->entry = scene->entry;
    m->ie = scene->ie;
    m->irq_at = scene->irq_at;
    m->nmi = scene->nmi;
    for (i = 0; i < scene->nins; i++) {
        const SIns *ins = &scene->ins[i];
        m->mem[ins->at] = (uint16_t)((ins->op << 12) | (ins->arg & 0xFF));
    }
    for (i = 0; i < scene->ndata; i++) {
        m->mem[scene->data[i].at] = (uint16_t)scene->data[i].val;
    }
}

static void print_head(FILE *out, const Scene *scene) {
    int i;
    fprintf(out, "【%s】按字编址\n", scene->title);
    fprintf(out, "初态 IF=%d 用户态 入口=%d\n", scene->ie, scene->entry);
    if (scene->irq_at < 0) {
        fprintf(out, "外部中断 无\n");
    } else {
        fprintf(out, "外部中断 %s 在字地址 %d 执行期间到达\n",
                scene->nmi ? "不可屏蔽" : "可屏蔽", scene->irq_at);
    }
    for (i = 0; i < scene->nins; i++) {
        char text[32];
        uint16_t word = (uint16_t)((scene->ins[i].op << 12) | (scene->ins[i].arg & 0xFF));
        format_inst(text, sizeof text, word);
        fprintf(out, "%d %s\n", scene->ins[i].at, text);
    }
    for (i = 0; i < scene->ndata; i++) {
        fprintf(out, "M[%d]=%d\n", scene->data[i].at, scene->data[i].val);
    }
}

static const Scene k_fault = {
    "fault", "除零故障", 6, 1, -1, 0, 9, 3,
    {{0, OP_LDA, 20}, {1, OP_DIV, 21}, {2, OP_STA, 22}, {3, OP_HLT, 0},
     {6, OP_STA, 24}, {7, OP_LDA, 23}, {8, OP_STA, 21}, {9, OP_LDA, 24}, {10, OP_IRET, 0}},
    {{20, 8}, {21, 0}, {23, 2}}
};

static const Scene k_trap = {
    "trap", "系统调用", 6, 1, -1, 0, 5, 1,
    {{0, OP_LDA, 10}, {1, OP_TRAP, 0}, {2, OP_HLT, 0}, {6, OP_NOP, 0}, {7, OP_IRET, 0}},
    {{10, 7}}
};

static const Scene k_abort = {
    "abort", "硬件故障", 6, 1, -1, 0, 1, 0,
    {{0, OP_FAIL, 0}},
    {{0, 0}}
};

static const Scene k_irq = {
    "irq", "可屏蔽中断", 6, 1, 1, 0, 5, 1,
    {{0, OP_LDA, 10}, {1, OP_ADD, 10}, {2, OP_HLT, 0}, {6, OP_NOP, 0}, {7, OP_IRET, 0}},
    {{10, 5}}
};

static const Scene k_mask = {
    "mask", "关中断挡住可屏蔽请求", 6, 0, 0, 0, 3, 0,
    {{0, OP_NOP, 0}, {1, OP_NOP, 0}, {2, OP_HLT, 0}},
    {{0, 0}}
};

static const Scene k_nmi = {
    "nmi", "不可屏蔽中断", 6, 0, 0, 1, 3, 0,
    {{0, OP_NOP, 0}, {1, OP_HLT, 0}, {6, OP_IRET, 0}},
    {{0, 0}}
};

static const Scene *find_scene(const char *name) {
    static const Scene *all[] = {&k_fault, &k_trap, &k_abort, &k_irq, &k_mask, &k_nmi};
    int i;
    for (i = 0; i < 6; i++) {
        if (strcmp(all[i]->name, name) == 0) return all[i];
    }
    return NULL;
}

int show_scene(FILE *out, const char *name, SceneResult *result) {
    const Scene *scene = find_scene(name);
    Mach mach;
    int rc;
    if (scene == NULL) {
        return 1;
    }
    print_head(out, scene);
    load_scene(&mach, scene, out);
    rc = run_mach(&mach);
    fill_result(&mach, result);
    if (rc != 0) return -1;
    fprintf(out, "结果：ACC=%u PC=%u 断点=%u 响应=%d 次 IF=%d\n",
            mach.acc, mach.pc, mach.saved, mach.took, mach.ie);
    return 0;
}
