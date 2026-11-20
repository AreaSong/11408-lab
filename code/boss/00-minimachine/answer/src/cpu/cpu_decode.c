#include "cpu/cpu_decode.h"

#include <stdio.h>
#include <string.h>

uint32_t cpu_enc_r(int rd, int rs, int rt, int shamt, int funct) {
    return ((uint32_t)(rd & 7) << 23) | ((uint32_t)(rs & 7) << 20) |
           ((uint32_t)(rt & 7) << 17) | ((uint32_t)(shamt & 31) << 6) |
           (uint32_t)(funct & 63);
}

uint32_t cpu_enc_i(int op, int rd, int rs, uint16_t imm) {
    return ((uint32_t)(op & 63) << 26) | ((uint32_t)(rd & 7) << 23) |
           ((uint32_t)(rs & 7) << 20) | imm;
}

uint32_t cpu_enc_m(int op, int rd, int rx, int mode, uint16_t disp) {
    return ((uint32_t)(op & 63) << 26) | ((uint32_t)(rd & 7) << 23) |
           ((uint32_t)(rx & 7) << 20) | ((uint32_t)(mode & 7) << 17) | disp;
}

uint32_t cpu_enc_b(int op, int cond, uint16_t disp) {
    return ((uint32_t)(op & 63) << 26) | ((uint32_t)(cond & 15) << 22) | disp;
}

static void fill_fields(uint32_t word, CpuInsn *in) {
    memset(in, 0, sizeof *in);
    in->word = word;
    in->op = (int)((word >> 26) & 63u);
    in->rd = (int)((word >> 23) & 7u);
    in->rs = (int)((word >> 20) & 7u);
    in->rt = (int)((word >> 17) & 7u);
    in->mode = in->rt;
    in->cond = (int)((word >> 22) & 15u);
    in->shamt = (int)((word >> 6) & 31u);
    in->funct = (int)(word & 63u);
    in->imm = (uint16_t)(word & 0xffffu);
}

static void check_r(CpuInsn *in) {
    if (((in->word >> 11) & 63u) != 0) {
        in->illegal = 1;
        return;
    }
    switch (in->funct) {
    case MM_FN_NOP:
        in->illegal = in->word != 0;
        break;
    case MM_FN_ADD:
    case MM_FN_SUB:
    case MM_FN_AND:
    case MM_FN_OR:
    case MM_FN_XOR:
    case MM_FN_MUL:
    case MM_FN_DIV:
    case MM_FN_MOD:
        in->illegal = in->shamt != 0;
        break;
    case MM_FN_NOT:
    case MM_FN_NEG:
    case MM_FN_MOV:
        in->illegal = in->rt != 0 || in->shamt != 0;
        break;
    case MM_FN_SHL:
    case MM_FN_SHR:
    case MM_FN_SAR:
        in->illegal = in->rt != 0;
        break;
    case MM_FN_CMP:
        in->illegal = in->rd != 0 || in->shamt != 0;
        break;
    case MM_FN_JR:
    case MM_FN_CALLR:
        in->illegal = in->rd != 0 || in->rt != 0 || in->shamt != 0;
        break;
    default:
        in->illegal = 1;
        break;
    }
}

static void check_i(CpuInsn *in) {
    if (((in->word >> 16) & 15u) != 0) {
        in->illegal = 1;
        return;
    }
    if (in->op == MM_OP_CMPI && in->rd != 0) {
        in->illegal = 1;
    }
    if (in->op == MM_OP_LUI && in->rs != 0) {
        in->illegal = 1;
    }
}

static void check_m(CpuInsn *in) {
    int mode = in->mode;
    if (((in->word >> 16) & 1u) != 0 || mode > MM_MODE_REL) {
        in->illegal = 1;
        return;
    }
    if (mode == MM_MODE_IMM && (in->op != MM_OP_LD || in->rs != 0)) {
        in->illegal = 1;
    }
    if ((mode == MM_MODE_DIR || mode == MM_MODE_IND || mode == MM_MODE_REL) && in->rs != 0) {
        in->illegal = 1;
    }
    if (mode == MM_MODE_REG && in->imm != 0) {
        in->illegal = 1;
    }
}

static void check_b(CpuInsn *in) {
    if (((in->word >> 16) & 63u) != 0) {
        in->illegal = 1;
        return;
    }
    if (in->op == MM_OP_JCC && in->cond > MM_COND_JNO) {
        in->illegal = 1;
    }
    if (in->op == MM_OP_CALL && in->cond != 0) {
        in->illegal = 1;
    }
    if ((in->op == MM_OP_TRAPV || in->op == MM_OP_BRK) && (in->cond != 0 || in->imm != 0)) {
        in->illegal = 1;
    }
}

static void check_s(CpuInsn *in) {
    if (((in->word >> 16) & 15u) != 0) {
        in->illegal = 1;
        return;
    }
    switch (in->op) {
    case MM_OP_SYSCALL:
    case MM_OP_IRET:
    case MM_OP_EI:
    case MM_OP_DI:
    case MM_OP_HLT:
    case MM_OP_TLBFLUSH:
        in->illegal = (in->word & 0x03ffffffu) != 0;
        break;
    case MM_OP_IN:
    case MM_OP_MFSR:
        in->illegal = in->rs != 0;
        break;
    case MM_OP_OUT:
    case MM_OP_MTSR:
        in->illegal = in->rd != 0;
        break;
    default:
        in->illegal = 1;
        break;
    }
    if ((in->op == MM_OP_MFSR || in->op == MM_OP_MTSR) && in->imm > MM_SR_PSW) {
        in->illegal = 1;
    }
}

int cpu_decode(uint32_t word, CpuInsn *out) {
    fill_fields(word, out);
    if (out->op == MM_OP_R) {
        check_r(out);
    } else if (out->op >= MM_OP_ADDI && out->op <= MM_OP_LUI) {
        check_i(out);
    } else if (out->op >= MM_OP_LD && out->op <= MM_OP_XCHG) {
        check_m(out);
    } else if (out->op >= MM_OP_JCC && out->op <= MM_OP_BRK) {
        check_b(out);
    } else if (out->op >= MM_OP_SYSCALL && out->op <= MM_OP_TLBFLUSH) {
        check_s(out);
    } else {
        out->illegal = 1;
    }
    return out->illegal ? -1 : 0;
}
