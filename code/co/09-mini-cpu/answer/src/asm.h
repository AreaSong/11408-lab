#ifndef ASM_H
#define ASM_H

#include <stddef.h>
#include <stdint.h>

#define MEM_WORDS 256
#define MAX_CODE 64
#define MAX_STEPS 32

enum {
    OP_NOP = 0,
    OP_LDA = 1,
    OP_STA = 2,
    OP_ADD = 3,
    OP_JMP = 4,
    OP_JZ = 5,
    OP_ADM = 6,
    OP_HLT = 7
};

int parse_num(const char *text, long *out);
int asm_encode(const char *line, uint16_t *word);
void fmt_inst(char *buf, size_t n, uint16_t word);
void fmt_hex(char *buf, size_t n, uint16_t word);

#endif
