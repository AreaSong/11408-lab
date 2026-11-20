#ifndef MM_MACHINE_H
#define MM_MACHINE_H

#include "common/mm_types.h"

#include <stdio.h>

typedef struct MmMachine MmMachine;
typedef struct AsmImage AsmImage;

/* size 是 1 或 4。返回 0 表示成功，否则返回异常向量号。 */
typedef int (*MmMemHook)(MmMachine *m, uint32_t addr, int size, uint32_t *data);

MmMachine *mm_create(uint32_t mem_bytes);
void mm_destroy(MmMachine *m);
void mm_reset(MmMachine *m);

int mm_load(MmMachine *m, const AsmImage *img);
int mm_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mm_write(MmMachine *m, uint32_t addr, int size, uint32_t data);
int mm_raw_read(MmMachine *m, uint32_t addr, int size, uint32_t *data);
int mm_raw_write(MmMachine *m, uint32_t addr, int size, uint32_t data);

uint32_t mm_reg(const MmMachine *m, int index);
void mm_set_reg(MmMachine *m, int index, uint32_t value);
uint32_t mm_pc(const MmMachine *m);
void mm_set_pc(MmMachine *m, uint32_t pc);
uint32_t mm_psw(const MmMachine *m);
void mm_set_psw(MmMachine *m, uint32_t psw);
uint32_t mm_sr(const MmMachine *m, int index);
void mm_set_sr(MmMachine *m, int index, uint32_t value);
uint32_t mm_badva(const MmMachine *m);

int mm_halted(const MmMachine *m);
int mm_cause(const MmMachine *m);
const char *mm_cause_name(int cause);
uint32_t mm_cycles(const MmMachine *m);
uint32_t mm_insn_count(const MmMachine *m);
int mm_bus_conflicts(const MmMachine *m);
uint32_t mm_mem_size(const MmMachine *m);

void mm_set_trace(MmMachine *m, FILE *fp);
int mm_set_hook(MmMachine *m, int id, MmMemHook fn);

void mm_print_regs(const MmMachine *m, FILE *fp);
void mm_print_stats(const MmMachine *m, FILE *fp);

#endif
