#ifndef X86_INT_H
#define X86_INT_H

#include <stdint.h>

#define X86_INS 48

enum { X_MOV, X_ADD, X_SUB, X_CMP, X_SHL, X_IMUL, X_PUSH, X_POP, X_JUMP, X_CALL, X_RET };

typedef struct {
    int op;
    int jcc;
    int rd;
    int rs;
    int mem_dst;
    int mem_src;
    int mem_reg;
    int32_t disp;
    uint32_t imm;
    int imm_ok;
    int target;
    char label[20];
    char tname[20];
    uint32_t addr;
    char text[80];
} X86Insn;

int x86_load(const char *const *lines, int n, X86Insn *prog, int *count);

#endif
