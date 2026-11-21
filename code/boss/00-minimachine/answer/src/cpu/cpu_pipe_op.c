#include "cpu/cpu_pipe_i.h"

#include "cpu/cpu_alu.h"
#include "cpu/cpu_intr.h"
#include "mmu/mmu_walk.h"

#include <string.h>

uint32_t pipe_gpr(const MmMachine *m, const PipeSlot *mem, int reg) {
    if (reg < 0 || reg > 7) {
        return 0;
    }
    /* load 的数要到 MEM 段末才有，同一拍不能转给 EX。ALU 结果上一拍已经锁进槽。 */
    if (mem != NULL && mem->valid && mem->fault < 0 && mem->dest == reg && !mem->is_load) {
        return mem->result;
    }
    return m->r[reg];
}

uint32_t pipe_psw(const MmMachine *m, const PipeSlot *mem) {
    uint32_t psw = m->psw & 0xffu;
    if (mem == NULL || !mem->valid || mem->fault >= 0) {
        return psw;
    }
    if (mem->psw_write) {
        return mem->new_psw & 0xffu;
    }
    if (mem->writes_flags) {
        return (psw & ~0x0fu) | (mem->flag_bits & 0x0fu);
    }
    return psw;
}


static void use_flags(PipeSlot *ex, CpuAlu alu) {
    ex->result = alu.result;
    ex->flag_bits = 0;
    if (alu.zf) {
        ex->flag_bits |= MM_PSW_ZF;
    }
    if (alu.sf) {
        ex->flag_bits |= MM_PSW_SF;
    }
    if (alu.cf) {
        ex->flag_bits |= MM_PSW_CF;
    }
    if (alu.of) {
        ex->flag_bits |= MM_PSW_OF;
    }
}

static void ex_alu(PipeSlot *ex, uint32_t rs, uint32_t rt, uint32_t psw) {
    const CpuInsn *in = &ex->in;
    uint32_t imm = in->imm;
    uint32_t se = (uint32_t)mm_sext16(in->imm);
    int old_cf = (psw & MM_PSW_CF) != 0;
    if (in->op == MM_OP_LUI) {
        ex->result = imm << 16;
        return;
    }
    if (in->op == MM_OP_ADDI) {
        use_flags(ex, cpu_alu_add(rs, se));
    } else if (in->op == MM_OP_SUBI || in->op == MM_OP_CMPI) {
        use_flags(ex, cpu_alu_sub(rs, se));
    } else if (in->op == MM_OP_ANDI) {
        use_flags(ex, cpu_alu_logic(rs & imm));
    } else if (in->op == MM_OP_ORI) {
        use_flags(ex, cpu_alu_logic(rs | imm));
    } else if (in->op == MM_OP_XORI) {
        use_flags(ex, cpu_alu_logic(rs ^ imm));
    } else if (in->funct == MM_FN_ADD) {
        use_flags(ex, cpu_alu_add(rs, rt));
    } else if (in->funct == MM_FN_SUB || in->funct == MM_FN_CMP) {
        use_flags(ex, cpu_alu_sub(rs, rt));
    } else if (in->funct == MM_FN_AND) {
        use_flags(ex, cpu_alu_logic(rs & rt));
    } else if (in->funct == MM_FN_OR) {
        use_flags(ex, cpu_alu_logic(rs | rt));
    } else if (in->funct == MM_FN_XOR) {
        use_flags(ex, cpu_alu_logic(rs ^ rt));
    } else if (in->funct == MM_FN_NOT) {
        use_flags(ex, cpu_alu_logic(~rs));
    } else if (in->funct == MM_FN_NEG) {
        use_flags(ex, cpu_alu_sub(0, rs));
    } else if (in->funct == MM_FN_SHL) {
        use_flags(ex, cpu_alu_shift(rs, in->shamt, CPU_SHL, old_cf));
    } else if (in->funct == MM_FN_SHR) {
        use_flags(ex, cpu_alu_shift(rs, in->shamt, CPU_SHR, old_cf));
    } else if (in->funct == MM_FN_SAR) {
        use_flags(ex, cpu_alu_shift(rs, in->shamt, CPU_SAR, old_cf));
    } else if (in->funct == MM_FN_MUL) {
        use_flags(ex, cpu_alu_mul(rs, rt));
    } else if (in->funct == MM_FN_MOV) {
        ex->result = rs;
    }
}

static void ex_div(PipeSlot *ex, uint32_t rs, uint32_t rt) {
    uint32_t quot = 0;
    if (cpu_alu_div(rs, rt, ex->in.funct == MM_FN_MOD, &quot) != 0) {
        ex->fault = MM_EX_DIV;
        ex->badva = ex->pc;
        ex->trap = 0;
        ex->taken = 0;
        return;
    }
    ex->result = quot;
}

static int bad_target(PipeSlot *ex, uint32_t psw) {
    if ((ex->target & 3u) != 0) {
        ex->fault = MM_EX_ALIGN;
        ex->badva = ex->target;
    } else if ((psw & MM_PSW_U) != 0 && cpu_is_host(ex->target)) {
        ex->fault = MM_EX_PROT;
        ex->badva = ex->target;
    } else {
        return 0;
    }
    ex->trap = 0;
    ex->taken = 0;
    return 1;
}

static void ex_ctrl(MmMachine *m, PipeSlot *ex, uint32_t rs, uint32_t psw) {
    const CpuInsn *in = &ex->in;
    if (in->op == MM_OP_JCC || in->op == MM_OP_CALL) {
        ex->target = ex->npc + (uint32_t)mm_sext16(in->imm);
        ex->taken = in->op == MM_OP_CALL || in->cond == MM_COND_JMP || cpu_cond(in->cond, psw);
    } else if (in->funct == MM_FN_JR || in->funct == MM_FN_CALLR) {
        ex->target = rs;
        ex->taken = 1;
    } else if (in->op == MM_OP_IRET) {
        ex->target = m->sr[MM_SR_EPC];
        ex->new_psw = m->sr[MM_SR_EPSW] & 0xffu;
        ex->taken = 1;
    } else if (in->op == MM_OP_EI) {
        ex->new_psw = (psw | MM_PSW_IF) & 0xffu;
    } else if (in->op == MM_OP_DI) {
        ex->new_psw = psw & ~MM_PSW_IF;
    } else if (in->op == MM_OP_TRAPV) {
        if ((psw & MM_PSW_OF) != 0) {
            ex->fault = MM_EX_OVF;
            ex->trap = 1;
        }
        return;
    } else if (in->op == MM_OP_MTSR) {
        ex->result = rs;
        return;
    }
    if (in->op == MM_OP_CALL || in->funct == MM_FN_CALLR) {
        ex->result = ex->npc;
    }
    if (ex->taken) {
        bad_target(ex, psw);
    }
}

static int is_ctrl(const CpuInsn *in) {
    return in->op == MM_OP_JCC || in->op == MM_OP_CALL || in->op == MM_OP_TRAPV || in->op == MM_OP_IRET ||
           in->op == MM_OP_EI || in->op == MM_OP_DI || in->op == MM_OP_MTSR ||
           (in->op == MM_OP_R && (in->funct == MM_FN_JR || in->funct == MM_FN_CALLR));
}

static void ex_ea(PipeSlot *ex, uint32_t rs) {
    const CpuInsn *in = &ex->in;
    uint32_t se = (uint32_t)mm_sext16(in->imm);
    if (in->op == MM_OP_LD && in->mode == MM_MODE_IMM) {
        ex->result = se;
        return;
    }
    if (in->mode == MM_MODE_DIR || in->mode == MM_MODE_IND) {
        ex->ea = in->imm;
    } else if (in->mode == MM_MODE_REG) {
        ex->ea = rs;
    } else if (in->mode == MM_MODE_BASE) {
        ex->ea = rs + se;
    } else if (in->mode == MM_MODE_IDX) {
        ex->ea = rs + in->imm;
    } else if (in->mode == MM_MODE_REL) {
        ex->ea = ex->npc + se;
    }
    if (!ex->uses_mem) {
        ex->result = ex->ea;
    }
}

void pipe_work_ex(MmMachine *m, PipeSlot *ex, const PipeSlot *mem) {
    uint32_t rs;
    uint32_t rt;
    uint32_t psw;
    if (ex == NULL || !ex->valid || ex->fault >= 0) {
        return;
    }
    rs = pipe_gpr(m, mem, ex->ex0);
    rt = pipe_gpr(m, mem, ex->ex1);
    psw = pipe_psw(m, mem);
    if (ex->in.op == MM_OP_R && (ex->in.funct == MM_FN_DIV || ex->in.funct == MM_FN_MOD)) {
        ex_div(ex, rs, rt);
        return;
    }
    if (ex->in.op == MM_OP_R || (ex->in.op >= MM_OP_ADDI && ex->in.op <= MM_OP_LUI)) {
        if (ex->in.op != MM_OP_R || ex->in.funct != MM_FN_NOP) {
            ex_alu(ex, rs, rt, psw);
        }
        return;
    }
    if (ex->in.op >= MM_OP_LD && ex->in.op <= MM_OP_XCHG) {
        ex_ea(ex, rs);
        return;
    }
    if (is_ctrl(&ex->in)) {
        ex_ctrl(m, ex, rs, psw);
    }
}

static int touch(MmMachine *m, PipeSlot *s, uint32_t addr, int size, int writing, uint32_t *data) {
    int vec;
    if (size == 4 && (addr & 3u) != 0) {
        s->fault = MM_EX_ALIGN;
        s->badva = addr;
        s->taken = 0;
        return -1;
    }
    /* 只置了 PG、还没装 MMU 时，保持阶段 02：数据访问直接缺页，取指仍走物理地址。 */
    if ((m->psw & MM_PSW_PG) != 0 && m->mmu == NULL) {
        s->fault = MM_EX_PAGE;
        s->badva = addr;
        s->taken = 0;
        return -1;
    }
    vec = writing ? mmu_mem_write(m, addr, size, *data) : mmu_mem_read(m, addr, size, data);
    if (vec != 0) {
        s->fault = vec;
        s->badva = addr;
        s->taken = 0;
        return -1;
    }
    return 0;
}

static int indirect(MmMachine *m, PipeSlot *s) {
    uint32_t ptr = 0;
    if (s->in.mode != MM_MODE_IND) {
        return 0;
    }
    if (touch(m, s, s->ea, 4, 0, &ptr) != 0) {
        return -1;
    }
    s->ea = ptr;
    return 0;
}

static void mem_load(MmMachine *m, PipeSlot *s) {
    uint32_t data = 0;
    int size = (s->in.op == MM_OP_LDB || s->in.op == MM_OP_LDBU) ? 1 : 4;
    if (s->in.op == MM_OP_LEA) {
        if (touch(m, s, s->ea, 4, 0, &data) == 0) {
            s->result = data;
        }
        return;
    }
    if (indirect(m, s) != 0 || touch(m, s, s->ea, size, 0, &data) != 0) {
        return;
    }
    if (s->in.op == MM_OP_LDB) {
        s->result = (uint32_t)(int32_t)(int8_t)(data & 0xffu);
    } else if (s->in.op == MM_OP_LDBU) {
        s->result = data & 0xffu;
    } else {
        s->result = data;
    }
}

static void mem_store(MmMachine *m, PipeSlot *s) {
    uint32_t data = 0;
    int size = s->in.op == MM_OP_STB ? 1 : 4;
    if (s->memr >= 0 && s->memr < 8) {
        data = m->r[s->memr];
    }
    if (indirect(m, s) != 0) {
        return;
    }
    if (s->in.op == MM_OP_XCHG) {
        uint32_t old = 0;
        if (touch(m, s, s->ea, 4, 0, &old) == 0 && touch(m, s, s->ea, 4, 1, &data) == 0) {
            s->result = old;
        }
        return;
    }
    if (s->in.op == MM_OP_TSL) {
        uint32_t old = 0;
        uint32_t one = 1;
        if (touch(m, s, s->ea, 4, 0, &old) == 0 && touch(m, s, s->ea, 4, 1, &one) == 0) {
            s->result = old;
        }
        return;
    }
    touch(m, s, s->ea, size, 1, &data);
}

void pipe_work_mem(MmMachine *m, PipeSlot *s) {
    if (s == NULL || !s->valid || s->fault >= 0) {
        return;
    }
    if (s->in.op == MM_OP_IN) {
        s->result = m->port[s->in.imm];
        return;
    }
    if (s->in.op == MM_OP_MFSR) {
        s->result = mm_sr(m, (int)s->in.imm);
        return;
    }
    if (s->in.op == MM_OP_OUT) {
        m->port[s->in.imm] = m->r[s->memr];
        if (s->in.imm == 0x70u) {
            cpu_eoi(m, (int)m->r[s->memr]);
        }
        if (m->after_out != NULL) {
            m->after_out(m, s->in.imm);
        }
        return;
    }
    if (!s->uses_mem) {
        return;
    }
    if (s->in.op == MM_OP_ST || s->in.op == MM_OP_STB || s->in.op == MM_OP_XCHG || s->in.op == MM_OP_TSL) {
        mem_store(m, s);
    } else {
        mem_load(m, s);
    }
    if (s->fault < 0 && m->after_mem != NULL) {
        m->after_mem(m);
    }
}

void pipe_work_wb(MmMachine *m, const PipeSlot *s) {
    if (s == NULL || !s->valid || s->fault >= 0) {
        return;
    }
    if (s->dest >= 0 && s->dest < 8) {
        m->r[s->dest] = s->result;
    }
    if (s->psw_write) {
        m->psw = s->new_psw & 0xffu;
    } else if (s->writes_flags) {
        m->psw = ((m->psw & ~0x0fu) | (s->flag_bits & 0x0fu)) & 0xffu;
    }
    if (s->in.op == MM_OP_MTSR) {
        mm_set_sr(m, (int)s->in.imm, s->result);
    }
    if (s->in.op == MM_OP_TLBFLUSH) {
        mmu_flush(m);
    }
}
