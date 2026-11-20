#ifndef MM_TYPES_H
#define MM_TYPES_H

#include <stdint.h>

/* MM32 字长与寻址空间。阶段 00 的主存和汇编映像都不超过 1MB，按字节编址。 */
#define MM_MEM_MAX (1u << 20)

#define MM_PSW_ZF  (1u << 0)
#define MM_PSW_SF  (1u << 1)
#define MM_PSW_CF  (1u << 2)
#define MM_PSW_OF  (1u << 3)
#define MM_PSW_IF  (1u << 4)
#define MM_PSW_U   (1u << 5)
#define MM_PSW_PG  (1u << 6)
#define MM_PSW_TF  (1u << 7)

/* 主操作码。R 型再用 funct 展开，这就是扩展操作码。 */
enum {
    MM_OP_R = 0x00,
    MM_OP_ADDI = 0x01,
    MM_OP_SUBI = 0x02,
    MM_OP_ANDI = 0x03,
    MM_OP_ORI = 0x04,
    MM_OP_XORI = 0x05,
    MM_OP_CMPI = 0x06,
    MM_OP_LUI = 0x07,
    MM_OP_LD = 0x10,
    MM_OP_ST = 0x11,
    MM_OP_LDB = 0x12,
    MM_OP_LDBU = 0x13,
    MM_OP_STB = 0x14,
    MM_OP_LEA = 0x15,
    MM_OP_TSL = 0x16,
    MM_OP_XCHG = 0x17,
    MM_OP_JCC = 0x20,
    MM_OP_CALL = 0x21,
    MM_OP_TRAPV = 0x22,
    MM_OP_BRK = 0x23,
    MM_OP_SYSCALL = 0x30,
    MM_OP_IRET = 0x31,
    MM_OP_EI = 0x32,
    MM_OP_DI = 0x33,
    MM_OP_HLT = 0x34,
    MM_OP_IN = 0x35,
    MM_OP_OUT = 0x36,
    MM_OP_MFSR = 0x37,
    MM_OP_MTSR = 0x38,
    MM_OP_TLBFLUSH = 0x39
};

enum {
    MM_FN_NOP = 0x00,
    MM_FN_ADD = 0x01,
    MM_FN_SUB = 0x02,
    MM_FN_AND = 0x03,
    MM_FN_OR = 0x04,
    MM_FN_XOR = 0x05,
    MM_FN_NOT = 0x06,
    MM_FN_NEG = 0x07,
    MM_FN_SHL = 0x08,
    MM_FN_SHR = 0x09,
    MM_FN_SAR = 0x0A,
    MM_FN_MUL = 0x0C,
    MM_FN_DIV = 0x0D,
    MM_FN_MOD = 0x0E,
    MM_FN_CMP = 0x10,
    MM_FN_MOV = 0x11,
    MM_FN_JR = 0x12,
    MM_FN_CALLR = 0x13
};

enum {
    MM_MODE_IMM = 0,
    MM_MODE_DIR = 1,
    MM_MODE_IND = 2,
    MM_MODE_REG = 3,
    MM_MODE_BASE = 4,
    MM_MODE_IDX = 5,
    MM_MODE_REL = 6
};

enum {
    MM_COND_JMP = 0,
    MM_COND_JE = 1,
    MM_COND_JNE = 2,
    MM_COND_JL = 3,
    MM_COND_JGE = 4,
    MM_COND_JLE = 5,
    MM_COND_JG = 6,
    MM_COND_JB = 7,
    MM_COND_JAE = 8,
    MM_COND_JBE = 9,
    MM_COND_JA = 10,
    MM_COND_JS = 11,
    MM_COND_JNS = 12,
    MM_COND_JO = 13,
    MM_COND_JNO = 14
};

enum {
    MM_SR_EPC = 0,
    MM_SR_EPSW = 1,
    MM_SR_CAUSE = 2,
    MM_SR_BADVA = 3,
    MM_SR_PTBR = 4,
    MM_SR_IMR = 5,
    MM_SR_KSP = 6,
    MM_SR_VBR = 7,
    MM_SR_PSW = 8
};

enum {
    MM_EX_DIV = 0,
    MM_EX_BREAK = 1,
    MM_EX_NMI = 2,
    MM_EX_ILL = 3,
    MM_EX_PRIV = 4,
    MM_EX_PAGE = 5,
    MM_EX_PROT = 6,
    MM_EX_ALIGN = 7,
    MM_EX_SYSCALL = 8,
    MM_EX_OVF = 9,
    MM_EX_MCHK = 10
};

/* 后面的阶段用这两个号注册访存钩子，编号只增不改。 */
#define MM_HOOK_READ  1
#define MM_HOOK_WRITE 2

static inline int32_t mm_sext16(uint16_t value) {
    return (int32_t)(int16_t)value;
}

#endif
