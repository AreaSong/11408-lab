#include "asm/asm_internal.h"

#include "cpu/cpu_decode.h"

#include <ctype.h>
#include <string.h>

enum {
    P_NONE, P_R3, P_R2, P_SH, P_CMP, P_JREG,
    P_I3, P_ICMP, P_LUI, P_MEM, P_TGT,
    P_IN, P_OUT, P_MF, P_MT,
    P_LI, P_LA, P_PUSH, P_POP, P_RET
};

typedef struct {
    const char *name;
    int op;
    int funct;
    int cond;
    int pat;
} Mnem;

static const Mnem TABLE[] = {
    {"nop", MM_OP_R, MM_FN_NOP, 0, P_NONE},
    {"add", MM_OP_R, MM_FN_ADD, 0, P_R3},
    {"sub", MM_OP_R, MM_FN_SUB, 0, P_R3},
    {"and", MM_OP_R, MM_FN_AND, 0, P_R3},
    {"or", MM_OP_R, MM_FN_OR, 0, P_R3},
    {"xor", MM_OP_R, MM_FN_XOR, 0, P_R3},
    {"mul", MM_OP_R, MM_FN_MUL, 0, P_R3},
    {"div", MM_OP_R, MM_FN_DIV, 0, P_R3},
    {"mod", MM_OP_R, MM_FN_MOD, 0, P_R3},
    {"not", MM_OP_R, MM_FN_NOT, 0, P_R2},
    {"neg", MM_OP_R, MM_FN_NEG, 0, P_R2},
    {"mov", MM_OP_R, MM_FN_MOV, 0, P_R2},
    {"shl", MM_OP_R, MM_FN_SHL, 0, P_SH},
    {"shr", MM_OP_R, MM_FN_SHR, 0, P_SH},
    {"sar", MM_OP_R, MM_FN_SAR, 0, P_SH},
    {"cmp", MM_OP_R, MM_FN_CMP, 0, P_CMP},
    {"jr", MM_OP_R, MM_FN_JR, 0, P_JREG},
    {"callr", MM_OP_R, MM_FN_CALLR, 0, P_JREG},
    {"addi", MM_OP_ADDI, 0, 0, P_I3},
    {"subi", MM_OP_SUBI, 0, 0, P_I3},
    {"andi", MM_OP_ANDI, 0, 0, P_I3},
    {"ori", MM_OP_ORI, 0, 0, P_I3},
    {"xori", MM_OP_XORI, 0, 0, P_I3},
    {"cmpi", MM_OP_CMPI, 0, 0, P_ICMP},
    {"lui", MM_OP_LUI, 0, 0, P_LUI},
    {"ld", MM_OP_LD, 0, 0, P_MEM},
    {"ldb", MM_OP_LDB, 0, 0, P_MEM},
    {"ldbu", MM_OP_LDBU, 0, 0, P_MEM},
    {"lea", MM_OP_LEA, 0, 0, P_MEM},
    {"tsl", MM_OP_TSL, 0, 0, P_MEM},
    {"xchg", MM_OP_XCHG, 0, 0, P_MEM},
    {"st", MM_OP_ST, 0, 0, P_MEM},
    {"stb", MM_OP_STB, 0, 0, P_MEM},
    {"jmp", MM_OP_JCC, 0, MM_COND_JMP, P_TGT},
    {"je", MM_OP_JCC, 0, MM_COND_JE, P_TGT},
    {"jne", MM_OP_JCC, 0, MM_COND_JNE, P_TGT},
    {"jl", MM_OP_JCC, 0, MM_COND_JL, P_TGT},
    {"jge", MM_OP_JCC, 0, MM_COND_JGE, P_TGT},
    {"jle", MM_OP_JCC, 0, MM_COND_JLE, P_TGT},
    {"jg", MM_OP_JCC, 0, MM_COND_JG, P_TGT},
    {"jb", MM_OP_JCC, 0, MM_COND_JB, P_TGT},
    {"jae", MM_OP_JCC, 0, MM_COND_JAE, P_TGT},
    {"jbe", MM_OP_JCC, 0, MM_COND_JBE, P_TGT},
    {"ja", MM_OP_JCC, 0, MM_COND_JA, P_TGT},
    {"js", MM_OP_JCC, 0, MM_COND_JS, P_TGT},
    {"jns", MM_OP_JCC, 0, MM_COND_JNS, P_TGT},
    {"jo", MM_OP_JCC, 0, MM_COND_JO, P_TGT},
    {"jno", MM_OP_JCC, 0, MM_COND_JNO, P_TGT},
    {"call", MM_OP_CALL, 0, 0, P_TGT},
    {"trapv", MM_OP_TRAPV, 0, 0, P_NONE},
    {"brk", MM_OP_BRK, 0, 0, P_NONE},
    {"syscall", MM_OP_SYSCALL, 0, 0, P_NONE},
    {"iret", MM_OP_IRET, 0, 0, P_NONE},
    {"ei", MM_OP_EI, 0, 0, P_NONE},
    {"di", MM_OP_DI, 0, 0, P_NONE},
    {"hlt", MM_OP_HLT, 0, 0, P_NONE},
    {"tlbflush", MM_OP_TLBFLUSH, 0, 0, P_NONE},
    {"in", MM_OP_IN, 0, 0, P_IN},
    {"out", MM_OP_OUT, 0, 0, P_OUT},
    {"mfsr", MM_OP_MFSR, 0, 0, P_MF},
    {"mtsr", MM_OP_MTSR, 0, 0, P_MT},
    {"li", 0, 0, 0, P_LI},
    {"la", 0, 0, 0, P_LA},
    {"push", 0, 0, 0, P_PUSH},
    {"pop", 0, 0, 0, P_POP},
    {"ret", 0, 0, 0, P_RET},
};

static void lower_copy(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static const Mnem *find_mnem(const char *src) {
    char name[24];
    size_t i;
    lower_copy(name, sizeof name, src);
    if (strcmp(name, "jz") == 0) {
        snprintf(name, sizeof name, "%s", "je");
    } else if (strcmp(name, "jnz") == 0) {
        snprintf(name, sizeof name, "%s", "jne");
    }
    for (i = 0; i < sizeof TABLE / sizeof TABLE[0]; i++) {
        if (strcmp(TABLE[i].name, name) == 0) {
            return &TABLE[i];
        }
    }
    return NULL;
}

static int need_n(int got, int want, char *err, size_t cap) {
    if (got != want) {
        mm_err(err, cap, "操作数个数应为 %d", want);
        return -1;
    }
    return 0;
}

static int need_reg(const char *text, int *reg, char *err, size_t cap) {
    if (!asm_is_reg(text, reg)) {
        mm_err(err, cap, "需要寄存器，得到 %s", text);
        return -1;
    }
    return 0;
}

static int need_imm(const char *text, const SymTab *tab, int64_t *out, char *err, size_t cap) {
    if (text[0] != '#') {
        mm_err(err, cap, "立即数要以 # 开头");
        return -1;
    }
    return asm_atom(text + 1, tab, out, err, cap);
}

static int s16(int64_t v, uint16_t *out, char *err, size_t cap) {
    if (v < -32768 || v > 32767) {
        mm_err(err, cap, "超出 16 位有符号范围");
        return -1;
    }
    *out = (uint16_t)(int16_t)v;
    return 0;
}

static int u16(int64_t v, uint16_t *out, char *err, size_t cap) {
    if (v < 0 || v > 65535) {
        mm_err(err, cap, "超出 16 位无符号范围");
        return -1;
    }
    *out = (uint16_t)v;
    return 0;
}

static int bits32(int64_t v, uint32_t *out, char *err, size_t cap) {
    if (v < (int64_t)INT32_MIN || v > 4294967295LL) {
        mm_err(err, cap, "立即数超出 32 位");
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

static int finish(EncOut *out, uint32_t pc, char *err, size_t cap) {
    int i;
    for (i = 0; i < out->n; i++) {
        if (cpu_disasm(out->w[i], pc + (uint32_t)i * 4u, out->text[i], sizeof out->text[i]) != 0) {
            mm_err(err, cap, "编码结果是非法指令");
            return -1;
        }
    }
    return 0;
}

static int enc_mem(const Mnem *m, int rd, const MemOp *mem, uint32_t pc, uint32_t *word,
                   char *err, size_t cap) {
    int64_t field = mem->rel_target ? mem->imm - (int64_t)(pc + 4u) : mem->imm;
    uint16_t raw = 0;
    int rx = mem->rx;
    if (mem->mode == MM_MODE_IMM && m->op != MM_OP_LD) {
        mm_err(err, cap, "只有 ld 能用立即寻址");
        return -1;
    }
    if (mem->mode == MM_MODE_IMM || mem->mode == MM_MODE_BASE || mem->mode == MM_MODE_REL) {
        if (s16(field, &raw, err, cap) != 0) {
            return -1;
        }
    } else if (mem->mode == MM_MODE_DIR || mem->mode == MM_MODE_IND || mem->mode == MM_MODE_IDX) {
        if (u16(mem->imm, &raw, err, cap) != 0) {
            return -1;
        }
    }
    *word = cpu_enc_m(m->op, rd, rx, mem->mode, raw);
    return 0;
}

static int enc_target(const Mnem *m, const char *arg, uint32_t pc, const SymTab *tab,
                      uint32_t *word, char *err, size_t cap) {
    int64_t target = 0;
    int64_t disp;
    uint16_t raw = 0;
    if (asm_atom(arg, tab, &target, err, cap) != 0) {
        return -1;
    }
    if ((target & 3) != 0) {
        mm_err(err, cap, "转移目标必须 4 字节对齐");
        return -1;
    }
    disp = target - (int64_t)(pc + 4u);
    if (s16(disp, &raw, err, cap) != 0) {
        return -1;
    }
    *word = cpu_enc_b(m->op, m->cond, raw);
    return 0;
}

static int enc_li(int rd, int64_t imm, EncOut *out, char *err, size_t cap) {
    uint32_t bits = 0;
    if (bits32(imm, &bits, err, cap) != 0) {
        return -1;
    }
    if ((uint32_t)mm_sext16((uint16_t)bits) == bits) {
        out->n = 1;
        out->w[0] = cpu_enc_m(MM_OP_LD, rd, 0, MM_MODE_IMM, (uint16_t)bits);
        return 0;
    }
    out->n = 2;
    out->w[0] = cpu_enc_i(MM_OP_LUI, rd, 0, (uint16_t)(bits >> 16));
    out->w[1] = cpu_enc_i(MM_OP_ORI, rd, rd, (uint16_t)bits);
    return 0;
}

static int encode_body(const Mnem *m, char op[][80], int n, uint32_t pc, const SymTab *tab,
                       EncOut *out, char *err, size_t cap) {
    int a = 0;
    int b = 0;
    int c = 0;
    int64_t imm = 0;
    uint16_t raw = 0;
    MemOp mem;
    out->n = 1;
    switch (m->pat) {
    case P_NONE:
        if (need_n(n, 0, err, cap) != 0) return -1;
        out->w[0] = m->op == MM_OP_R ? 0 : cpu_enc_i(m->op, 0, 0, 0);
        return 0;
    case P_R3:
        if (need_n(n, 3, err, cap) || need_reg(op[0], &a, err, cap) || need_reg(op[1], &b, err, cap) ||
            need_reg(op[2], &c, err, cap)) return -1;
        out->w[0] = cpu_enc_r(a, b, c, 0, m->funct);
        return 0;
    case P_R2:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || need_reg(op[1], &b, err, cap)) return -1;
        out->w[0] = cpu_enc_r(a, b, 0, 0, m->funct);
        return 0;
    case P_SH:
        if (need_n(n, 3, err, cap) || need_reg(op[0], &a, err, cap) || need_reg(op[1], &b, err, cap) ||
            need_imm(op[2], tab, &imm, err, cap) || imm < 0 || imm > 31) {
            if (err == NULL || err[0] == '\0') {
                mm_err(err, cap, "移位位数要在 0 到 31");
            }
            return -1;
        }
        out->w[0] = cpu_enc_r(a, b, 0, (int)imm, m->funct);
        return 0;
    case P_CMP:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || need_reg(op[1], &b, err, cap)) return -1;
        out->w[0] = cpu_enc_r(0, a, b, 0, MM_FN_CMP);
        return 0;
    case P_JREG:
        if (need_n(n, 1, err, cap) || need_reg(op[0], &a, err, cap)) return -1;
        out->w[0] = cpu_enc_r(0, a, 0, 0, m->funct);
        return 0;
    case P_I3:
        if (need_n(n, 3, err, cap) || need_reg(op[0], &a, err, cap) || need_reg(op[1], &b, err, cap) ||
            need_imm(op[2], tab, &imm, err, cap)) return -1;
        if ((m->op == MM_OP_ANDI || m->op == MM_OP_ORI || m->op == MM_OP_XORI) ? u16(imm, &raw, err, cap)
                                                                              : s16(imm, &raw, err, cap)) return -1;
        out->w[0] = cpu_enc_i(m->op, a, b, raw);
        return 0;
    case P_ICMP:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || need_imm(op[1], tab, &imm, err, cap) ||
            s16(imm, &raw, err, cap)) return -1;
        out->w[0] = cpu_enc_i(MM_OP_CMPI, 0, a, raw);
        return 0;
    case P_LUI:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || need_imm(op[1], tab, &imm, err, cap) ||
            u16(imm, &raw, err, cap)) return -1;
        out->w[0] = cpu_enc_i(MM_OP_LUI, a, 0, raw);
        return 0;
    case P_MEM:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) ||
            asm_parse_mem(op[1], tab, &mem, err, cap) || enc_mem(m, a, &mem, pc, &out->w[0], err, cap)) return -1;
        return 0;
    case P_TGT:
        if (need_n(n, 1, err, cap) || enc_target(m, op[0], pc, tab, &out->w[0], err, cap)) return -1;
        return 0;
    case P_IN:
    case P_MF:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || need_imm(op[1], tab, &imm, err, cap) ||
            u16(imm, &raw, err, cap)) return -1;
        if (m->pat == P_MF && imm > MM_SR_PSW) {
            mm_err(err, cap, "控制寄存器号超出范围");
            return -1;
        }
        out->w[0] = cpu_enc_i(m->op, a, 0, raw);
        return 0;
    case P_OUT:
    case P_MT:
        if (need_n(n, 2, err, cap) || need_imm(op[0], tab, &imm, err, cap) || need_reg(op[1], &a, err, cap) ||
            u16(imm, &raw, err, cap)) return -1;
        if (m->pat == P_MT && imm > MM_SR_PSW) {
            mm_err(err, cap, "控制寄存器号超出范围");
            return -1;
        }
        out->w[0] = cpu_enc_i(m->op, 0, a, raw);
        return 0;
    case P_LI:
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || need_imm(op[1], tab, &imm, err, cap)) return -1;
        return enc_li(a, imm, out, err, cap);
    case P_LA: {
        uint32_t addr = 0;
        if (need_n(n, 2, err, cap) || need_reg(op[0], &a, err, cap) || asm_atom(op[1], tab, &imm, err, cap) ||
            bits32(imm, &addr, err, cap)) {
            return -1;
        }
        out->n = 2;
        out->w[0] = cpu_enc_i(MM_OP_LUI, a, 0, (uint16_t)(addr >> 16));
        out->w[1] = cpu_enc_i(MM_OP_ORI, a, a, (uint16_t)addr);
        return 0;
    }
    case P_PUSH:
        if (need_n(n, 1, err, cap) || need_reg(op[0], &a, err, cap)) return -1;
        out->n = 2;
        out->w[0] = cpu_enc_i(MM_OP_SUBI, 7, 7, 4);
        out->w[1] = cpu_enc_m(MM_OP_ST, a, 7, MM_MODE_REG, 0);
        return 0;
    case P_POP:
        if (need_n(n, 1, err, cap) || need_reg(op[0], &a, err, cap)) return -1;
        out->n = 2;
        out->w[0] = cpu_enc_m(MM_OP_LD, a, 7, MM_MODE_REG, 0);
        out->w[1] = cpu_enc_i(MM_OP_ADDI, 7, 7, 4);
        return 0;
    case P_RET:
        if (need_n(n, 0, err, cap)) return -1;
        out->w[0] = cpu_enc_r(0, 6, 0, 0, MM_FN_JR);
        return 0;
    default:
        mm_err(err, cap, "内部错误");
        return -1;
    }
}

static int split_of(const char *args, char op[][80], int *n, char *err, size_t cap) {
    int got = args == NULL || args[0] == '\0' ? 0 : asm_split(args, op, 4);
    if (got < 0) {
        mm_err(err, cap, "操作数过多或过长");
        return -1;
    }
    *n = got;
    return 0;
}

int asm_encode(const char *mnem, const char *args, uint32_t pc, const SymTab *tab,
               EncOut *out, char *err, size_t cap) {
    const Mnem *m = find_mnem(mnem);
    char op[4][80];
    int n = 0;
    memset(out, 0, sizeof *out);
    if (err != NULL && cap > 0) {
        err[0] = '\0';
    }
    if (m == NULL) {
        mm_err(err, cap, "未知指令 %s", mnem);
        return -1;
    }
    if (split_of(args, op, &n, err, cap) != 0 || encode_body(m, op, n, pc, tab, out, err, cap) != 0) {
        return -1;
    }
    return finish(out, pc, err, cap);
}

int asm_width(const char *mnem, const char *args, const SymTab *tab, char *err, size_t cap) {
    const Mnem *m = find_mnem(mnem);
    char op[4][80];
    int n = 0;
    int64_t imm = 0;
    uint32_t bits = 0;
    if (m == NULL) {
        mm_err(err, cap, "未知指令 %s", mnem);
        return -1;
    }
    if (m->pat == P_LA || m->pat == P_PUSH || m->pat == P_POP) {
        return 8;
    }
    if (m->pat != P_LI) {
        return 4;
    }
    if (split_of(args, op, &n, err, cap) != 0 || need_n(n, 2, err, cap) != 0 ||
        need_imm(op[1], tab, &imm, err, cap) != 0 || bits32(imm, &bits, err, cap) != 0) {
        return -1;
    }
    return (uint32_t)mm_sext16((uint16_t)bits) == bits ? 4 : 8;
}
