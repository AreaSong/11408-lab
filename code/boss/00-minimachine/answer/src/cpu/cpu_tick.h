#ifndef CPU_TICK_H
#define CPU_TICK_H

#include "common/mm_internal.h"

enum {
    K_FETCH_MAR = 1,
    K_FETCH_MEM,
    K_FETCH_IR,
    K_DECODE,
    K_RS_Y,
    K_RX_Y,
    K_RX_MAR,
    K_RD_MDR,
    K_RS_RD,
    K_RS_PC,
    K_PC_Y,
    K_PC_SAVE,
    K_PC_SAVE_Y,
    K_Z_MAR,
    K_Z_RD,
    K_Z_PC,
    K_LK_R6,
    K_SE_RD,
    K_ZE_MAR,
    K_ZE_RD,
    K_MDR_RD,
    K_MDR_SE,
    K_MDR_ZE,
    K_MDR_MAR,
    K_MDR_Z,
    K_ONE_MDR,
    K_LUI,
    K_READ_W,
    K_READ_B,
    K_WRITE_W,
    K_WRITE_B,
    K_ALU_RT,
    K_ALU_SE,
    K_ALU_ZE,
    K_ADDR_SE,
    K_ADDR_ZE,
    K_NOT,
    K_NEG,
    K_SHIFT,
    K_MUL,
    K_DIV,
    K_MOD,
    K_BRANCH,
    K_EMPTY,
    K_HALT,
    K_EI,
    K_DI,
    K_IRET_PC,
    K_IRET_PSW,
    K_MFSR,
    K_MTSR,
    K_IN,
    K_OUT,
    K_TLB,
    K_TRAP
};

enum { TK_OK = 0, TK_HALT = 1, TK_FAULT = 2, TK_TRAP = 3 };

int cpu_tick_fetch(MmMachine *m, int kind);
int cpu_tick_move(MmMachine *m, int kind);
int cpu_tick_mem(MmMachine *m, int kind);
int cpu_tick(MmMachine *m, int kind);
int cpu_seq(const CpuInsn *in, int *kinds, int cap);
void cpu_emit_tick(MmMachine *m);

#endif
