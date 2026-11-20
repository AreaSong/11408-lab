#include "cpu/cpu_tick.h"

typedef struct {
    int *k;
    int n;
    int cap;
    int bad;
} Seq;

static void push(Seq *s, int kind) {
    if (s->bad || s->n >= s->cap) {
        s->bad = 1;
        return;
    }
    s->k[s->n++] = kind;
}

static void ea_mar(Seq *s, int mode) {
    switch (mode) {
    case MM_MODE_DIR:
        push(s, K_ZE_MAR);
        break;
    case MM_MODE_IND:
        push(s, K_ZE_MAR);
        push(s, K_READ_W);
        push(s, K_MDR_MAR);
        break;
    case MM_MODE_REG:
        push(s, K_RX_MAR);
        break;
    case MM_MODE_BASE:
        push(s, K_RX_Y);
        push(s, K_ADDR_SE);
        push(s, K_Z_MAR);
        break;
    case MM_MODE_IDX:
        push(s, K_RX_Y);
        push(s, K_ADDR_ZE);
        push(s, K_Z_MAR);
        break;
    case MM_MODE_REL:
        push(s, K_PC_Y);
        push(s, K_ADDR_SE);
        push(s, K_Z_MAR);
        break;
    default:
        s->bad = 1;
        break;
    }
}

static void seq_lea(Seq *s, int mode) {
    switch (mode) {
    case MM_MODE_DIR:
        push(s, K_ZE_RD);
        break;
    case MM_MODE_IND:
        push(s, K_ZE_MAR);
        push(s, K_READ_W);
        push(s, K_MDR_RD);
        break;
    case MM_MODE_REG:
        push(s, K_RS_RD);
        break;
    case MM_MODE_BASE:
        push(s, K_RX_Y);
        push(s, K_ADDR_SE);
        push(s, K_Z_RD);
        break;
    case MM_MODE_IDX:
        push(s, K_RX_Y);
        push(s, K_ADDR_ZE);
        push(s, K_Z_RD);
        break;
    case MM_MODE_REL:
        push(s, K_PC_Y);
        push(s, K_ADDR_SE);
        push(s, K_Z_RD);
        break;
    default:
        s->bad = 1;
        break;
    }
}

static void seq_m(Seq *s, const CpuInsn *in) {
    switch (in->op) {
    case MM_OP_LD:
        if (in->mode == MM_MODE_IMM) {
            push(s, K_SE_RD);
            break;
        }
        ea_mar(s, in->mode);
        push(s, K_READ_W);
        push(s, K_MDR_RD);
        break;
    case MM_OP_LDB:
        ea_mar(s, in->mode);
        push(s, K_READ_B);
        push(s, K_MDR_SE);
        break;
    case MM_OP_LDBU:
        ea_mar(s, in->mode);
        push(s, K_READ_B);
        push(s, K_MDR_ZE);
        break;
    case MM_OP_ST:
        ea_mar(s, in->mode);
        push(s, K_RD_MDR);
        push(s, K_WRITE_W);
        break;
    case MM_OP_STB:
        ea_mar(s, in->mode);
        push(s, K_RD_MDR);
        push(s, K_WRITE_B);
        break;
    case MM_OP_LEA:
        seq_lea(s, in->mode);
        break;
    case MM_OP_TSL:
        ea_mar(s, in->mode);
        push(s, K_READ_W);
        push(s, K_MDR_RD);
        push(s, K_ONE_MDR);
        push(s, K_WRITE_W);
        break;
    case MM_OP_XCHG:
        ea_mar(s, in->mode);
        push(s, K_READ_W);
        push(s, K_MDR_Z);
        push(s, K_RD_MDR);
        push(s, K_WRITE_W);
        push(s, K_Z_RD);
        break;
    default:
        s->bad = 1;
        break;
    }
}

static void seq_alu3(Seq *s, int funct) {
    push(s, K_RS_Y);
    push(s, K_ALU_RT);
    if (funct != MM_FN_CMP) {
        push(s, K_Z_RD);
    }
}

static void seq_r(Seq *s, const CpuInsn *in) {
    switch (in->funct) {
    case MM_FN_NOP:
        push(s, K_EMPTY);
        break;
    case MM_FN_ADD:
    case MM_FN_SUB:
    case MM_FN_AND:
    case MM_FN_OR:
    case MM_FN_XOR:
    case MM_FN_CMP:
        seq_alu3(s, in->funct);
        break;
    case MM_FN_NOT:
        push(s, K_NOT);
        push(s, K_Z_RD);
        break;
    case MM_FN_NEG:
        push(s, K_RS_Y);
        push(s, K_NEG);
        push(s, K_Z_RD);
        break;
    case MM_FN_SHL:
    case MM_FN_SHR:
    case MM_FN_SAR:
        push(s, K_SHIFT);
        push(s, K_Z_RD);
        break;
    case MM_FN_MUL:
        push(s, K_RS_Y);
        push(s, K_MUL);
        push(s, K_Z_RD);
        break;
    case MM_FN_DIV:
        push(s, K_RS_Y);
        push(s, K_DIV);
        push(s, K_Z_RD);
        break;
    case MM_FN_MOD:
        push(s, K_RS_Y);
        push(s, K_MOD);
        push(s, K_Z_RD);
        break;
    case MM_FN_MOV:
        push(s, K_RS_RD);
        break;
    case MM_FN_JR:
        push(s, K_RS_PC);
        break;
    case MM_FN_CALLR:
        push(s, K_PC_SAVE);
        push(s, K_RS_PC);
        push(s, K_LK_R6);
        break;
    default:
        s->bad = 1;
        break;
    }
}

static void seq_i(Seq *s, const CpuInsn *in) {
    switch (in->op) {
    case MM_OP_ADDI:
    case MM_OP_SUBI:
        push(s, K_RS_Y);
        push(s, K_ALU_SE);
        push(s, K_Z_RD);
        break;
    case MM_OP_ANDI:
    case MM_OP_ORI:
    case MM_OP_XORI:
        push(s, K_RS_Y);
        push(s, K_ALU_ZE);
        push(s, K_Z_RD);
        break;
    case MM_OP_CMPI:
        push(s, K_RS_Y);
        push(s, K_ALU_SE);
        break;
    case MM_OP_LUI:
        push(s, K_LUI);
        break;
    default:
        s->bad = 1;
        break;
    }
}

static void seq_b(Seq *s, const CpuInsn *in) {
    if (in->op == MM_OP_JCC) {
        push(s, K_PC_Y);
        push(s, K_ADDR_SE);
        push(s, K_BRANCH);
        return;
    }
    if (in->op == MM_OP_CALL) {
        push(s, K_PC_SAVE_Y);
        push(s, K_ADDR_SE);
        push(s, K_Z_PC);
        push(s, K_LK_R6);
        return;
    }
    push(s, K_TRAP);
}

static void seq_s(Seq *s, const CpuInsn *in) {
    switch (in->op) {
    case MM_OP_IRET:
        push(s, K_IRET_PC);
        push(s, K_IRET_PSW);
        break;
    case MM_OP_EI:
        push(s, K_EI);
        break;
    case MM_OP_DI:
        push(s, K_DI);
        break;
    case MM_OP_HLT:
        push(s, K_HALT);
        break;
    case MM_OP_IN:
        push(s, K_IN);
        break;
    case MM_OP_OUT:
        push(s, K_OUT);
        break;
    case MM_OP_MFSR:
        push(s, K_MFSR);
        break;
    case MM_OP_MTSR:
        push(s, K_MTSR);
        break;
    case MM_OP_TLBFLUSH:
        push(s, K_TLB);
        break;
    case MM_OP_SYSCALL:
        push(s, K_TRAP);
        break;
    default:
        s->bad = 1;
        break;
    }
}

int cpu_seq(const CpuInsn *in, int *kinds, int cap) {
    Seq s;
    s.k = kinds;
    s.n = 0;
    s.cap = cap;
    s.bad = 0;
    if (in->op == MM_OP_R) {
        seq_r(&s, in);
    } else if (in->op <= MM_OP_LUI) {
        seq_i(&s, in);
    } else if (in->op <= MM_OP_XCHG) {
        seq_m(&s, in);
    } else if (in->op <= MM_OP_BRK) {
        seq_b(&s, in);
    } else {
        seq_s(&s, in);
    }
    return s.bad ? -1 : s.n;
}
