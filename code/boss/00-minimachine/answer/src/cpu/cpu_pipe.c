#include "cpu/cpu_pipe.h"

#include "cpu/cpu_decode.h"
#include "cpu/cpu_intr.h"
#include "cpu/cpu_pipe_i.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_walk.h"

#include <string.h>

enum { P_IF, P_ID, P_EX, P_MEM, P_WB };

static void bubble(PipeSlot *s) {
    memset(s, 0, sizeof *s);
    s->dest = -1;
    s->ex0 = -1;
    s->ex1 = -1;
    s->memr = -1;
    s->fault = -1;
}

static int any_valid(const PipeSlot st[5]) {
    int i;
    for (i = 0; i < 5; i++) {
        if (st[i].valid) {
            return 1;
        }
    }
    return 0;
}

static void squash(MmMachine *m, PipeSlot *s) {
    if (s->valid && s->seq >= 0 && s->seq < MM_PIPE_LOG && !m->pipe_log_flush[s->seq]) {
        m->pipe_log_flush[s->seq] = 1;
        m->pipe_flushed++;
    }
    bubble(s);
}

static void remember(MmMachine *m, PipeSlot *s) {
    char text[48];
    if (m->pipe_nlog >= MM_PIPE_LOG) {
        m->pipe_overflow = 1;
        s->seq = -1;
        return;
    }
    s->seq = m->pipe_nlog++;
    m->pipe_log_pc[s->seq] = s->pc;
    m->pipe_log_branch[s->seq] = s->is_branch;
    if (cpu_disasm(s->in.word, s->pc, text, sizeof text) != 0) {
        snprintf(text, sizeof text, "非法");
    }
    snprintf(m->pipe_log_text[s->seq], sizeof m->pipe_log_text[s->seq], "%s", text);
}

static void punish(PipeSlot *s) {
    s->is_branch = 0;
    s->halt = 0;
    s->uses_mem = 0;
    s->is_load = 0;
    s->psw_write = 0;
    s->writes_flags = 0;
    s->dest = -1;
    s->memr = -1;
    s->taken = 0;
}

/* 取一条进 IF。预测不转移，所以 PC 先加 4；猜错再改回去。 */
static void fetch(MmMachine *m, PipeSlot *s) {
    uint32_t word = 0;
    int vec;
    bubble(s);
    s->valid = 1;
    s->pc = m->pc;
    s->npc = m->pc + 4u;
    m->pc = s->npc;
    if ((s->pc & 3u) != 0) {
        s->fault = MM_EX_ALIGN;
        s->badva = s->pc;
        remember(m, s);
        return;
    }
    if ((m->psw & MM_PSW_U) != 0 && cpu_is_host(s->pc)) {
        s->fault = MM_EX_PROT;
        s->badva = s->pc;
        remember(m, s);
        return;
    }
    m->mem_if = 1;
    vec = mmu_mem_read(m, s->pc, 4, &word);
    m->mem_if = 0;
    if (vec != 0) {
        s->fault = vec;
        s->badva = s->pc;
        remember(m, s);
        return;
    }
    cpu_decode(word, &s->in);
    pipe_classify(s);
    if (s->fault < 0 && cpu_is_privileged(s->in.op) && (m->psw & MM_PSW_U) != 0) {
        s->fault = MM_EX_PRIV;
        s->badva = s->pc;
        punish(s);
    }
    remember(m, s);
}

static int raw_stall(const PipeSlot st[5], int reg, int need_ex, int forward) {
    int stage = P_EX;
    int found = -1;
    int is_load = 0;
    for (; stage <= P_WB; stage++) {
        if (!st[stage].valid || st[stage].fault >= 0) {
            continue;
        }
        if (reg == -2) {
            if (st[stage].writes_flags || st[stage].psw_write) {
                found = stage;
                break;
            }
        } else if (st[stage].dest == reg) {
            found = stage;
            is_load = st[stage].is_load;
            break;
        }
    }
    if (found < 0) {
        return 0;
    }
    if (!forward) {
        return found != P_WB;
    }
    if (found == P_MEM || found == P_WB) {
        return 0;
    }
    return !(found == P_EX && !(is_load && need_ex));
}

static int id_stall(const MmMachine *m, const PipeSlot st[5]) {
    const PipeSlot *id = &st[P_ID];
    int regs[3];
    int early[3];
    int n = 0;
    int i;
    if (!id->valid || id->fault >= 0) {
        return 0;
    }
    if (id->ex0 >= 0) {
        regs[n] = id->ex0;
        early[n++] = 1;
    }
    if (id->ex1 >= 0) {
        regs[n] = id->ex1;
        early[n++] = 1;
    }
    if (id->memr >= 0) {
        regs[n] = id->memr;
        early[n++] = 0;
    }
    if (id->reads_flags) {
        regs[n] = -2;
        early[n++] = 1;
    }
    for (i = 0; i < n; i++) {
        if (raw_stall(st, regs[i], early[i], m->pipe_forward)) {
            return 1;
        }
    }
    return 0;
}

static int struct_block(const MmMachine *m, const PipeSlot st[5]) {
    return m->pipe_unified && st[P_EX].valid && st[P_EX].uses_mem && st[P_EX].fault < 0;
}

static int branch_block(const MmMachine *m, const PipeSlot st[5]) {
    int s;
    if (!m->pipe_branch) {
        return 0;
    }
    for (s = P_IF; s <= P_EX; s++) {
        if (st[s].valid && st[s].is_branch && st[s].fault < 0) {
            return 1;
        }
    }
    return 0;
}

static int hold_fetch(const PipeSlot st[5]) {
    int s;
    for (s = 0; s < 5; s++) {
        if (!st[s].valid) {
            continue;
        }
        if (st[s].fault >= 0 || st[s].halt) {
            return 1;
        }
    }
    return 0;
}

static void record(MmMachine *m, int cycle) {
    int s;
    if (cycle > m->pipe_clog) {
        m->pipe_clog = cycle;
    }
    for (s = 0; s < 5; s++) {
        int seq = m->pipe_st[s].seq;
        if (!m->pipe_st[s].valid || seq < 0 || seq >= MM_PIPE_LOG) {
            continue;
        }
        if (s == P_ID) {
            m->pipe_log_id[seq]++;
        }
        if (s == P_IF && m->pipe_log_if[seq] == 0) {
            m->pipe_log_if[seq] = cycle;
        }
        if (cycle > 0 && cycle < MM_PIPE_CYC) {
            m->pipe_cell[seq][cycle] = (char)(s + 1);
        } else {
            m->pipe_overflow = 1;
        }
    }
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

static void account(MmMachine *m, const PipeSlot *s) {
    int cat = category(&s->in);
    m->insns++;
    m->pipe_committed++;
    m->cat_n[cat]++;
    m->cat_c[cat] += 1;
}

static void trace_cycle(MmMachine *m, int cycle) {
    static const char *name[] = {"IF", "ID", "EX", "MEM", "WB"};
    int s;
    if (m->trace == NULL) {
        return;
    }
    fprintf(m->trace, "流水 C%d", cycle);
    for (s = 0; s < 5; s++) {
        if (m->pipe_st[s].valid) {
            fprintf(m->trace, " %s=%08X", name[s], m->pipe_st[s].pc);
        } else {
            fprintf(m->trace, " %s=—", name[s]);
        }
    }
    fputc('\n', m->trace);
}

static uint32_t younger_pc(const MmMachine *m) {
    int s;
    for (s = P_MEM; s >= P_IF; s--) {
        if (m->pipe_st[s].valid) {
            return m->pipe_st[s].pc;
        }
    }
    return m->pc;
}

static void blank_next(PipeSlot next[5]) {
    int i;
    for (i = 0; i < 5; i++) {
        bubble(&next[i]);
    }
}

static void divert_exc(MmMachine *m, PipeSlot next[5]) {
    PipeSlot *wb = &m->pipe_st[P_WB];
    int trap = wb->trap;
    int i;
    for (i = 0; i < 4; i++) {
        squash(m, &m->pipe_st[i]);
    }
    m->badva = wb->badva;
    m->ipc = wb->pc;
    m->pc = trap ? wb->npc : wb->pc;
    cpu_deliver(m, wb->fault, trap);
    if (m->after_intr != NULL) {
        m->after_intr(m);
    }
    blank_next(next);
    if (!m->halted) {
        fetch(m, &next[P_IF]);
    }
}

static int try_irq(MmMachine *m, PipeSlot next[5]) {
    uint32_t saved = m->pc;
    uint32_t epc = younger_pc(m);
    int i;
    m->pc = epc;
    m->ipc = epc;
    if (!cpu_poll(m)) {
        m->pc = saved;
        return 0;
    }
    for (i = 0; i < 4; i++) {
        squash(m, &m->pipe_st[i]);
    }
    if (m->after_intr != NULL) {
        m->after_intr(m);
    }
    blank_next(next);
    if (!m->halted) {
        fetch(m, &next[P_IF]);
    }
    return 1;
}

static void flow(MmMachine *m, PipeSlot next[5]) {
    PipeSlot *st = m->pipe_st;
    int blocked;
    if (!m->pipe_branch && st[P_EX].valid && st[P_EX].is_branch && st[P_EX].taken && st[P_EX].fault < 0) {
        squash(m, &st[P_IF]);
        squash(m, &st[P_ID]);
        m->pc = st[P_EX].target;
        next[P_WB] = st[P_MEM];
        next[P_MEM] = st[P_EX];
        fetch(m, &next[P_IF]);
        return;
    }
    if (id_stall(m, st)) {
        next[P_WB] = st[P_MEM];
        next[P_MEM] = st[P_EX];
        next[P_ID] = st[P_ID];
        next[P_IF] = st[P_IF];
        return;
    }
    if (m->pipe_branch && st[P_MEM].valid && st[P_MEM].is_branch && st[P_MEM].taken && st[P_MEM].fault < 0) {
        m->pc = st[P_MEM].target;
    }
    blocked = struct_block(m, st) || branch_block(m, st) || hold_fetch(st);
    if (struct_block(m, st) && !branch_block(m, st) && !hold_fetch(st)) {
        m->pipe_struct++;
    }
    next[P_WB] = st[P_MEM];
    next[P_MEM] = st[P_EX];
    next[P_EX] = st[P_ID];
    next[P_ID] = st[P_IF];
    if (!blocked && !m->halted) {
        fetch(m, &next[P_IF]);
    }
}

static int one_cycle(MmMachine *m, int *committed) {
    PipeSlot *st = m->pipe_st;
    PipeSlot next[5];
    int i;
    *committed = 0;
    blank_next(next);
    m->step_base = m->cycles;
    m->cycles++;
    m->pipe_cycles++;
    cpu_timer_edge(m);
    record(m, m->pipe_cycles);
    trace_cycle(m, m->pipe_cycles);
    if (st[P_WB].valid && st[P_WB].fault >= 0) {
        account(m, &st[P_WB]);
        *committed = 1;
        divert_exc(m, next);
    } else if (st[P_WB].valid) {
        pipe_work_wb(m, &st[P_WB]);
        account(m, &st[P_WB]);
        *committed = 1;
        if (st[P_WB].halt) {
            m->pc = st[P_WB].npc;
            for (i = 0; i < 4; i++) {
                squash(m, &st[i]);
            }
            m->halted = 1;
            m->asleep = 1;
            m->cause = -1;
        }
        if (!try_irq(m, next) && !m->halted) {
            if (st[P_MEM].valid) {
                pipe_work_mem(m, &st[P_MEM]);
            }
            if (st[P_EX].valid) {
                pipe_work_ex(m, &st[P_EX], &st[P_MEM]);
            }
            flow(m, next);
        }
    } else {
        if (st[P_MEM].valid) {
            pipe_work_mem(m, &st[P_MEM]);
        }
        if (st[P_EX].valid) {
            pipe_work_ex(m, &st[P_EX], &st[P_MEM]);
        }
        flow(m, next);
    }
    {
        int extra = mem_cache_take_stall(m) + mmu_take_stall(m);
        m->cycles += (uint32_t)extra;
        m->pipe_cycles += extra;
    }
    memcpy(st, next, sizeof next);
    return m->halted;
}

static void asleep_clock(MmMachine *m) {
    m->step_base = m->cycles;
    m->cycles++;
    m->pipe_cycles++;
    cpu_timer_edge(m);
    if (cpu_poll(m) && m->after_intr != NULL) {
        m->after_intr(m);
    }
    if (!m->halted && !any_valid(m->pipe_st)) {
        fetch(m, &m->pipe_st[P_IF]);
    }
}

static void finish_ctrl(MmMachine *m) {
    int i;
    m->pipe_data = 0;
    m->pipe_ctrl = 0;
    for (i = 0; i < m->pipe_nlog; i++) {
        int extra = m->pipe_log_id[i] > 0 ? m->pipe_log_id[i] - 1 : 0;
        m->pipe_data += extra;
        m->pipe_log_ctrl[i] = 0;
    }
    if (!m->pipe_branch) {
        return;
    }
    for (i = 1; i < m->pipe_nlog; i++) {
        int gap;
        int extra;
        int ctrl;
        if (!m->pipe_log_branch[i - 1] || m->pipe_log_flush[i] || m->pipe_log_flush[i - 1]) {
            continue;
        }
        if (m->pipe_log_if[i] == 0 || m->pipe_log_if[i - 1] == 0) {
            continue;
        }
        gap = m->pipe_log_if[i] - m->pipe_log_if[i - 1] - 1;
        extra = m->pipe_log_id[i - 1] > 0 ? m->pipe_log_id[i - 1] - 1 : 0;
        ctrl = gap - extra;
        if (ctrl < 0) {
            ctrl = 0;
        }
        m->pipe_log_ctrl[i] = ctrl;
        m->pipe_ctrl += ctrl;
    }
}


int cpu_pipe_run(MmMachine *m, int max_commit) {
    int got = 0;
    if (max_commit < 1) {
        max_commit = 1;
    }
    if (m->halted && m->asleep && !any_valid(m->pipe_st)) {
        asleep_clock(m);
    }
    if (m->halted) {
        finish_ctrl(m);
        return 0;
    }
    if (!any_valid(m->pipe_st)) {
        fetch(m, &m->pipe_st[P_IF]);
    }
    while (got < max_commit && m->pipe_cycles < 1000000) {
        int one = 0;
        if (one_cycle(m, &one)) {
            got += one;
            break;
        }
        got += one;
    }
    finish_ctrl(m);
    return m->halted ? 0 : 1;
}
