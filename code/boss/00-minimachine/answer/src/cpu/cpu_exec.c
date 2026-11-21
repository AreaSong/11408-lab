#include "cpu/cpu_exec.h"
#include "cpu/cpu_intr.h"
#include "cpu/cpu_pipe.h"
#include "cpu/cpu_tick.h"

#include <string.h>

static int run_tick(MmMachine *m, int kind) {
    int rc = cpu_tick(m, kind);
    cpu_emit_tick(m);
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

static int vector_empty(const MmMachine *m) {
    uint32_t word = 0;
    if (m->cause < 0 || m->cause >= MM_VEC_COUNT) {
        return 1;
    }
    if (mm_raw_read((MmMachine *)m, m->sr[MM_SR_VBR] + (uint32_t)m->cause * 4u, 4, &word) != 0) {
        return 0;
    }
    return word == 0;
}

static void drain(MmMachine *m) {
    cpu_timer_edge(m);
    while (!m->halted && cpu_poll(m)) {
    }
    if (m->after_intr != NULL) {
        m->after_intr(m);
    }
}

static int fetch_word(MmMachine *m) {
    int rc = run_tick(m, K_FETCH_MAR);
    if (rc == TK_OK) {
        rc = run_tick(m, K_FETCH_MEM);
    }
    if (rc == TK_OK) {
        rc = run_tick(m, K_FETCH_IR);
    }
    if (rc == TK_OK) {
        rc = run_tick(m, K_DECODE);
    }
    return rc;
}

static int execute_insn(MmMachine *m, int *trap) {
    int kinds[16];
    int n = 0;
    int i;
    int rc;
    int had = 0;
    m->t_index = 0;
    m->op_mem = 0;
    m->fetch_mem = 0;
    m->step_base = m->cycles;
    m->cause = -1;
    m->trap = 0;
    memset(&m->din, 0, sizeof m->din);
    m->din.illegal = 1;
    rc = fetch_word(m);
    if (rc != TK_OK) {
        had = 1;
    } else if (m->din.illegal) {
        m->cause = MM_EX_ILL;
        had = 1;
    } else if (cpu_is_privileged(m->din.op) && (m->psw & MM_PSW_U) != 0) {
        m->cause = MM_EX_PRIV;
        had = 1;
    } else {
        n = cpu_seq(&m->din, kinds, 16);
        if (n < 0) {
            m->cause = MM_EX_ILL;
            had = 1;
        }
        for (i = 0; !had && i < n; i++) {
            rc = run_tick(m, kinds[i]);
            if (rc == TK_HALT) {
                m->halted = 1;
                m->asleep = 1;
                m->cause = -1;
                break;
            }
            if (rc == TK_FAULT) {
                had = 1;
            } else if (rc == TK_TRAP) {
                had = 1;
                *trap = 1;
            }
        }
    }
    return had;
}

static void settle(MmMachine *m, int had, int trap) {
    if (had && vector_empty(m)) {
        cpu_deliver(m, m->cause, trap);
        account(m);
    } else {
        account(m);
        if (had) {
            cpu_deliver(m, m->cause, trap);
        } else {
            drain(m);
        }
    }
}

int cpu_step(MmMachine *m) {
    int trap = 0;
    int had;
    if (m->pipe_on) {
        if (m->halted && !m->asleep) {
            return 1;
        }
        cpu_pipe_run(m, 1);
        return (m->halted && !m->asleep) ? 1 : 0;
    }
    if (m->halted && !m->asleep) {
        return 1;
    }
    if (m->asleep) {
        drain(m);
        return (m->halted && !m->asleep) ? 1 : 0;
    }
    had = execute_insn(m, &trap);
    settle(m, had, trap);
    if (m->k_step != NULL && !m->halted && m->irq_depth == 0 && m->deliver_depth == 0) {
        m->k_step(m);
    }
    return (m->halted && !m->asleep) ? 1 : 0;
}

int cpu_run(MmMachine *m, int max_steps) {
    int i;
    if (m->pipe_on) {
        return cpu_pipe_run(m, max_steps);
    }
    for (i = 0; i < max_steps; i++) {
        if (m->halted && !m->asleep) {
            return 0;
        }
        cpu_step(m);
        if (m->halted) {
            return 0;
        }
    }
    return 1;
}
