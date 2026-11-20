#ifndef CPU_PIPE_I_H
#define CPU_PIPE_I_H

#include "common/mm_internal.h"

void pipe_classify(PipeSlot *s);
uint32_t pipe_gpr(const MmMachine *m, const PipeSlot *mem, int reg);
uint32_t pipe_psw(const MmMachine *m, const PipeSlot *mem);
void pipe_work_ex(MmMachine *m, PipeSlot *ex, const PipeSlot *mem);
void pipe_work_mem(MmMachine *m, PipeSlot *s);
void pipe_work_wb(MmMachine *m, const PipeSlot *s);

#endif
