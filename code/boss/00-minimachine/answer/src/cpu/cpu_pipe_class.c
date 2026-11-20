#include "cpu/cpu_pipe_i.h"

static void blank(PipeSlot *s) {
    s->dest = -1;
    s->ex0 = -1;
    s->ex1 = -1;
    s->memr = -1;
    s->is_load = 0;
    s->uses_mem = 0;
    s->is_branch = 0;
    s->writes_flags = 0;
    s->reads_flags = 0;
    s->halt = 0;
    s->psw_write = 0;
    s->trap = 0;
    s->taken = 0;
    s->fault = -1;
}

static void pair(PipeSlot *s, int rs, int rt, int rd, int flags) {
    s->ex0 = rs;
    s->ex1 = rt;
    s->dest = rd;
    s->writes_flags = flags;
}

static void class_r(PipeSlot *s) {
    const CpuInsn *in = &s->in;
    switch (in->funct) {
    case MM_FN_NOP:
        break;
    case MM_FN_ADD:
    case MM_FN_SUB:
    case MM_FN_AND:
    case MM_FN_OR:
    case MM_FN_XOR:
    case MM_FN_MUL:
        pair(s, in->rs, in->rt, in->rd, 1);
        break;
    case MM_FN_CMP:
        pair(s, in->rs, in->rt, -1, 1);
        break;
    case MM_FN_NOT:
    case MM_FN_NEG:
    case MM_FN_SHL:
    case MM_FN_SHR:
    case MM_FN_SAR:
        pair(s, in->rs, -1, in->rd, 1);
        break;
    case MM_FN_DIV:
    case MM_FN_MOD:
        pair(s, in->rs, in->rt, in->rd, 0);
        break;
    case MM_FN_MOV:
        pair(s, in->rs, -1, in->rd, 0);
        break;
    case MM_FN_JR:
        s->is_branch = 1;
        s->ex0 = in->rs;
        break;
    case MM_FN_CALLR:
        s->is_branch = 1;
        s->dest = 6;
        s->ex0 = in->rs;
        break;
    default:
        s->fault = MM_EX_ILL;
        break;
    }
}

static void class_mem(PipeSlot *s) {
    const CpuInsn *in = &s->in;
    int store = in->op == MM_OP_ST || in->op == MM_OP_STB || in->op == MM_OP_XCHG;
    int load = in->op != MM_OP_ST && in->op != MM_OP_STB;
    if (in->mode == MM_MODE_IMM) {
        s->fault = in->op == MM_OP_LD ? -1 : MM_EX_ILL;
        s->dest = in->op == MM_OP_LD ? in->rd : -1;
        return;
    }
    if (in->mode == MM_MODE_REG || in->mode == MM_MODE_BASE || in->mode == MM_MODE_IDX) {
        s->ex0 = in->rs;
    }
    if (in->op == MM_OP_LEA && in->mode != MM_MODE_IND) {
        s->dest = in->rd;
        return;
    }
    s->uses_mem = 1;
    if (load) {
        s->is_load = 1;
        s->dest = in->rd;
    }
    if (store) {
        s->memr = in->rd;
    }
}

static void class_sys(PipeSlot *s);

static void class_ctrl(PipeSlot *s) {
    const CpuInsn *in = &s->in;
    if (in->op == MM_OP_JCC) {
        s->is_branch = 1;
        s->reads_flags = in->cond != MM_COND_JMP;
        return;
    }
    if (in->op == MM_OP_CALL) {
        s->is_branch = 1;
        s->dest = 6;
        return;
    }
    if (in->op == MM_OP_TRAPV) {
        s->reads_flags = 1;
        return;
    }
    if (in->op == MM_OP_BRK) {
        s->fault = MM_EX_BREAK;
        s->trap = 1;
        return;
    }
    class_sys(s);
}

static void class_sys(PipeSlot *s) {
    const CpuInsn *in = &s->in;
    if (in->op == MM_OP_SYSCALL) {
        s->fault = MM_EX_SYSCALL;
        s->trap = 1;
        return;
    }
    if (in->op == MM_OP_IRET) {
        s->is_branch = 1;
        s->psw_write = 1;
        return;
    }
    if (in->op == MM_OP_EI || in->op == MM_OP_DI) {
        s->psw_write = 1;
        return;
    }
    if (in->op == MM_OP_HLT) {
        s->halt = 1;
        return;
    }
    if (in->op == MM_OP_IN || in->op == MM_OP_MFSR) {
        s->is_load = 1;
        s->dest = in->rd;
        return;
    }
    if (in->op == MM_OP_OUT) {
        s->memr = in->rs;
        return;
    }
    if (in->op == MM_OP_MTSR) {
        s->ex0 = in->rs;
        return;
    }
    if (in->op == MM_OP_TLBFLUSH) {
        return;
    }
    s->fault = MM_EX_ILL;
}

static void class_op(PipeSlot *s) {
    const CpuInsn *in = &s->in;
    if (in->op == MM_OP_R) {
        class_r(s);
        return;
    }
    if (in->op >= MM_OP_ADDI && in->op <= MM_OP_CMPI) {
        pair(s, in->rs, -1, in->op == MM_OP_CMPI ? -1 : in->rd, 1);
        return;
    }
    if (in->op == MM_OP_LUI) {
        s->dest = in->rd;
        return;
    }
    if (in->op >= MM_OP_LD && in->op <= MM_OP_XCHG) {
        class_mem(s);
        return;
    }
    class_ctrl(s);
}

void pipe_classify(PipeSlot *s) {
    blank(s);
    if (s->in.illegal) {
        s->fault = MM_EX_ILL;
        return;
    }
    class_op(s);
}
