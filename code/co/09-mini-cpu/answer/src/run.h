#ifndef RUN_H
#define RUN_H

#include "asm.h"
#include "psw.h"

#include <stdio.h>

typedef struct {
    uint16_t code[MAX_CODE];
    int ncode;
    uint16_t daddr[MAX_CODE];
    uint16_t dval[MAX_CODE];
    int ndata;
    uint16_t reg[4];
    int reg_mask;
} Program;

typedef struct {
    uint16_t pc;
    uint16_t ir;
    uint16_t mar;
    uint16_t mdr;
    uint16_t acc;
    uint16_t a;
    uint16_t r[4];
    uint16_t mem[MEM_WORDS];
    uint8_t dirty[MEM_WORDS];
    Psw psw;
    int visits;
    int conflicts;
    int insns;
    int halted;
} Cpu;

int cpu_exec(const Program *prog, Cpu *cpu, FILE *out);

#endif
