#include "cpu/cpu_decode.h"

#include <stdarg.h>
#include <stdio.h>

static int putf(char *buf, size_t cap, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

static const char *funct_name(int funct) {
    switch (funct) {
    case MM_FN_NOP: return "nop";
    case MM_FN_ADD: return "add";
    case MM_FN_SUB: return "sub";
    case MM_FN_AND: return "and";
    case MM_FN_OR: return "or";
    case MM_FN_XOR: return "xor";
    case MM_FN_NOT: return "not";
    case MM_FN_NEG: return "neg";
    case MM_FN_SHL: return "shl";
    case MM_FN_SHR: return "shr";
    case MM_FN_SAR: return "sar";
    case MM_FN_MUL: return "mul";
    case MM_FN_DIV: return "div";
    case MM_FN_MOD: return "mod";
    case MM_FN_CMP: return "cmp";
    case MM_FN_MOV: return "mov";
    case MM_FN_JR: return "jr";
    case MM_FN_CALLR: return "callr";
    default: return NULL;
    }
}

static const char *cond_name(int cond) {
    switch (cond) {
    case MM_COND_JMP: return "jmp";
    case MM_COND_JE: return "je";
    case MM_COND_JNE: return "jne";
    case MM_COND_JL: return "jl";
    case MM_COND_JGE: return "jge";
    case MM_COND_JLE: return "jle";
    case MM_COND_JG: return "jg";
    case MM_COND_JB: return "jb";
    case MM_COND_JAE: return "jae";
    case MM_COND_JBE: return "jbe";
    case MM_COND_JA: return "ja";
    case MM_COND_JS: return "js";
    case MM_COND_JNS: return "jns";
    case MM_COND_JO: return "jo";
    case MM_COND_JNO: return "jno";
    default: return NULL;
    }
}

static const char *op_name(int op) {
    switch (op) {
    case MM_OP_ADDI: return "addi";
    case MM_OP_SUBI: return "subi";
    case MM_OP_ANDI: return "andi";
    case MM_OP_ORI: return "ori";
    case MM_OP_XORI: return "xori";
    case MM_OP_CMPI: return "cmpi";
    case MM_OP_LUI: return "lui";
    case MM_OP_LD: return "ld";
    case MM_OP_ST: return "st";
    case MM_OP_LDB: return "ldb";
    case MM_OP_LDBU: return "ldbu";
    case MM_OP_STB: return "stb";
    case MM_OP_LEA: return "lea";
    case MM_OP_TSL: return "tsl";
    case MM_OP_XCHG: return "xchg";
    default: return NULL;
    }
}

static int dis_r(const CpuInsn *in, char *buf, size_t cap) {
    const char *name = funct_name(in->funct);
    if (name == NULL) {
        return -1;
    }
    switch (in->funct) {
    case MM_FN_NOP:
        return putf(buf, cap, "nop");
    case MM_FN_ADD:
    case MM_FN_SUB:
    case MM_FN_AND:
    case MM_FN_OR:
    case MM_FN_XOR:
    case MM_FN_MUL:
    case MM_FN_DIV:
    case MM_FN_MOD:
        return putf(buf, cap, "%s r%d, r%d, r%d", name, in->rd, in->rs, in->rt);
    case MM_FN_NOT:
    case MM_FN_NEG:
    case MM_FN_MOV:
        return putf(buf, cap, "%s r%d, r%d", name, in->rd, in->rs);
    case MM_FN_SHL:
    case MM_FN_SHR:
    case MM_FN_SAR:
        return putf(buf, cap, "%s r%d, r%d, #%d", name, in->rd, in->rs, in->shamt);
    case MM_FN_CMP:
        return putf(buf, cap, "cmp r%d, r%d", in->rs, in->rt);
    default:
        return putf(buf, cap, "%s r%d", name, in->rs);
    }
}

static int dis_i(const CpuInsn *in, char *buf, size_t cap) {
    const char *name = op_name(in->op);
    int32_t simm = mm_sext16(in->imm);
    if (name == NULL) {
        return -1;
    }
    if (in->op == MM_OP_CMPI) {
        return putf(buf, cap, "cmpi r%d, #%d", in->rs, simm);
    }
    if (in->op == MM_OP_LUI) {
        return putf(buf, cap, "lui r%d, #%u", in->rd, in->imm);
    }
    if (in->op == MM_OP_ANDI || in->op == MM_OP_ORI || in->op == MM_OP_XORI) {
        return putf(buf, cap, "%s r%d, r%d, #%u", name, in->rd, in->rs, in->imm);
    }
    return putf(buf, cap, "%s r%d, r%d, #%d", name, in->rd, in->rs, simm);
}

static int format_mem(const CpuInsn *in, char *mem, size_t cap) {
    int32_t simm = mm_sext16(in->imm);
    switch (in->mode) {
    case MM_MODE_IMM:
        return putf(mem, cap, "#%d", simm);
    case MM_MODE_DIR:
        return putf(mem, cap, "[0x%04X]", in->imm);
    case MM_MODE_IND:
        return putf(mem, cap, "[[0x%04X]]", in->imm);
    case MM_MODE_REG:
        return putf(mem, cap, "[r%d]", in->rs);
    case MM_MODE_BASE:
        return putf(mem, cap, "[r%d%+d]", in->rs, (int)simm);
    case MM_MODE_IDX:
        return putf(mem, cap, "0x%04X[r%d]", in->imm, in->rs);
    default:
        return putf(mem, cap, "[pc%+d]", (int)simm);
    }
}

static int dis_m(const CpuInsn *in, char *buf, size_t cap) {
    char mem[48];
    const char *name = op_name(in->op);
    if (name == NULL || format_mem(in, mem, sizeof mem) != 0) {
        return -1;
    }
    return putf(buf, cap, "%s r%d, %s", name, in->rd, mem);
}

static int dis_b(const CpuInsn *in, uint32_t pc, char *buf, size_t cap) {
    uint32_t target = pc + 4u + (uint32_t)mm_sext16(in->imm);
    const char *name;
    if (in->op == MM_OP_TRAPV) {
        return putf(buf, cap, "trapv");
    }
    if (in->op == MM_OP_BRK) {
        return putf(buf, cap, "brk");
    }
    if (in->op == MM_OP_CALL) {
        return putf(buf, cap, "call 0x%08X", target);
    }
    name = cond_name(in->cond);
    if (name == NULL) {
        return -1;
    }
    return putf(buf, cap, "%s 0x%08X", name, target);
}

static int dis_s(const CpuInsn *in, char *buf, size_t cap) {
    switch (in->op) {
    case MM_OP_SYSCALL: return putf(buf, cap, "syscall");
    case MM_OP_IRET: return putf(buf, cap, "iret");
    case MM_OP_EI: return putf(buf, cap, "ei");
    case MM_OP_DI: return putf(buf, cap, "di");
    case MM_OP_HLT: return putf(buf, cap, "hlt");
    case MM_OP_TLBFLUSH: return putf(buf, cap, "tlbflush");
    case MM_OP_IN: return putf(buf, cap, "in r%d, #%u", in->rd, in->imm);
    case MM_OP_OUT: return putf(buf, cap, "out #%u, r%d", in->imm, in->rs);
    case MM_OP_MFSR: return putf(buf, cap, "mfsr r%d, #%u", in->rd, in->imm);
    case MM_OP_MTSR: return putf(buf, cap, "mtsr #%u, r%d", in->imm, in->rs);
    default: return -1;
    }
}

int cpu_disasm(uint32_t word, uint32_t pc, char *buf, size_t cap) {
    CpuInsn in;
    if (buf == NULL || cap == 0) {
        return -1;
    }
    buf[0] = '\0';
    if (cpu_decode(word, &in) != 0) {
        return -1;
    }
    if (in.op == MM_OP_R) {
        return dis_r(&in, buf, cap);
    }
    if (in.op >= MM_OP_ADDI && in.op <= MM_OP_LUI) {
        return dis_i(&in, buf, cap);
    }
    if (in.op >= MM_OP_LD && in.op <= MM_OP_XCHG) {
        return dis_m(&in, buf, cap);
    }
    if (in.op >= MM_OP_JCC && in.op <= MM_OP_BRK) {
        return dis_b(&in, pc, buf, cap);
    }
    return dis_s(&in, buf, cap);
}
