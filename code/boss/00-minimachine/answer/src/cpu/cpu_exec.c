#include "cpu/cpu_exec.h"
#include "cpu/cpu_tick.h"

#include <string.h>

static void emit(MmMachine *m) {
    int drivers = cpu_bus_drivers(m->t_sig);
    m->t_index++;
    m->cycles++;
    if (drivers > 1) {
        m->bus_bad++;
    }
    if (m->trace == NULL) {
        return;
    }
    fprintf(m->trace, "%s T%d %s | %s | %s | 内总线驱动 %d 个\n", m->t_stage, m->t_index,
            m->t_func, m->t_detail, m->t_sig, drivers);
    if (drivers > 1) {
        fprintf(m->trace, "冲突：同一时钟周期只允许一个部件向内总线输出\n");
    }
}

static int run_tick(MmMachine *m, int kind) {
    int rc = cpu_tick(m, kind);
    emit(m);
    return rc;
}

static int category(const CpuInsn *in) {
    if (in->illegal || (in->op == MM_OP_R && in->funct == MM_FN_NOP)) {
        return 4;
    }
    if (in->op == MM_OP_R && (in->funct == MM_FN_JR || in->funct == MM_FN_CALLR)) {
        return 2;
    }
    if (in->op == MM_OP_R || (in->op >= MM_OP_ADDI && in->op <= MM_OP_LUI)) {
        return 0;
    }
    if (in->op >= MM_OP_LD && in->op <= MM_OP_XCHG) {
        return 1;
    }
    if (in->op == MM_OP_JCC || in->op == MM_OP_CALL) {
        return 2;
    }
    return 3;
}

static void account(MmMachine *m) {
    uint32_t used = m->cycles - m->step_base;
    int cat = category(&m->din);
    uint32_t psw = m->psw & 0xffu;
    int i;
    m->insns++;
    m->cat_n[cat]++;
    m->cat_c[cat] += used;
    if (m->trace == NULL) {
        return;
    }
    fprintf(m->trace, "状态 PC=%08X", m->pc);
    for (i = 0; i < 8; i++) {
        fprintf(m->trace, " r%d=%08X", i, m->r[i]);
    }
    fprintf(m->trace,
            " ZF=%d SF=%d CF=%d OF=%d IF=%d U=%d PG=%d 拍数=%d 取指访存=%d 取操作数访存=%d\n",
            (psw & MM_PSW_ZF) != 0, (psw & MM_PSW_SF) != 0, (psw & MM_PSW_CF) != 0,
            (psw & MM_PSW_OF) != 0, (psw & MM_PSW_IF) != 0, (psw & MM_PSW_U) != 0,
            (psw & MM_PSW_PG) != 0, m->t_index, m->fetch_mem, m->op_mem);
}

static void note_exc(MmMachine *m, int trap) {
    if (!trap) {
        m->pc = m->ipc;
    }
    m->halted = 1;
    m->trap = trap;
    if (m->trace == NULL || m->cause < 0) {
        return;
    }
    fprintf(m->trace, "异常 %s 向量=%d 断点=%s %08X\n", mm_cause_name(m->cause), m->cause,
            trap ? "下一条" : "本条", trap ? m->pc : m->ipc);
}

static int stop_exc(MmMachine *m, int trap) {
    note_exc(m, trap);
    account(m);
    return 1;
}

static int privileged(const CpuInsn *in) {
    switch (in->op) {
    case MM_OP_IRET:
    case MM_OP_EI:
    case MM_OP_DI:
    case MM_OP_HLT:
    case MM_OP_IN:
    case MM_OP_OUT:
    case MM_OP_MFSR:
    case MM_OP_MTSR:
    case MM_OP_TLBFLUSH:
        return 1;
    default:
        return 0;
    }
}

int cpu_step(MmMachine *m) {
    int kinds[16];
    int n;
    int i;
    int rc;
    if (m->halted) {
        return 1;
    }
    m->t_index = 0;
    m->op_mem = 0;
    m->fetch_mem = 0;
    m->step_base = m->cycles;
    m->cause = -1;
    m->trap = 0;
    memset(&m->din, 0, sizeof m->din);
    m->din.illegal = 1;
    rc = run_tick(m, K_FETCH_MAR);
    if (rc == TK_OK) {
        rc = run_tick(m, K_FETCH_MEM);
    }
    if (rc == TK_OK) {
        rc = run_tick(m, K_FETCH_IR);
    }
    if (rc == TK_OK) {
        rc = run_tick(m, K_DECODE);
    }
    if (rc != TK_OK) {
        return stop_exc(m, 0);
    }
    if (m->din.illegal) {
        m->cause = MM_EX_ILL;
        return stop_exc(m, 0);
    }
    if (privileged(&m->din) && (m->psw & MM_PSW_U) != 0) {
        m->cause = MM_EX_PRIV;
        return stop_exc(m, 0);
    }
    n = cpu_seq(&m->din, kinds, 16);
    if (n < 0) {
        m->cause = MM_EX_ILL;
        return stop_exc(m, 0);
    }
    for (i = 0; i < n; i++) {
        rc = run_tick(m, kinds[i]);
        if (rc == TK_HALT) {
            m->halted = 1;
            m->cause = -1;
            break;
        }
        if (rc == TK_FAULT) {
            return stop_exc(m, 0);
        }
        if (rc == TK_TRAP) {
            return stop_exc(m, 1);
        }
    }
    account(m);
    return m->halted ? 1 : 0;
}

int cpu_run(MmMachine *m, int max_steps) {
    int i;
    for (i = 0; i < max_steps; i++) {
        if (m->halted) {
            return 0;
        }
        cpu_step(m);
    }
    return m->halted ? 0 : 1;
}
