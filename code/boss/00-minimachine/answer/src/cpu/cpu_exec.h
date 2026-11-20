#ifndef CPU_EXEC_H
#define CPU_EXEC_H

#include "common/mm_machine.h"
#include "cpu/cpu_decode.h"

#include <stdint.h>

typedef struct {
    int ok;
    int has_ea;
    uint32_t ea;
    int mem_ops;
    int mode;
} CpuEa;

/* 执行一条指令。返回 0 表示还可以继续，返回 1 表示已停机或发生异常。 */
int cpu_step(MmMachine *m);

/* 最多执行 max_steps 条。返回 0 表示停机或异常，返回 1 表示步数用尽。 */
int cpu_run(MmMachine *m, int max_steps);

/*
 * 按 ISA 第 5 节计算 EA 和取操作数的访存次数。
 * rx 是变址/基址/寄存器间接时的寄存器值；mem_word 是一次间接时 M[zext(A)] 的字。
 * pc 是本条指令的地址（函数内部使用已经加 4 的 PC）。
 */
int cpu_ea(uint32_t word, uint32_t pc, uint32_t rx, uint32_t mem_word, CpuEa *out);

/* 统计控制信号里有几个内总线驱动。MDRoutE 走的是外部数据总线，不计入。 */
int cpu_bus_drivers(const char *signals);

#endif
