#ifndef CPU_PIPE_H
#define CPU_PIPE_H

#include "common/mm_machine.h"

#include <stdio.h>
#include <stdint.h>

/*
 * 五段流水线的开关。默认关，节拍模式的 trace 和周期数保持阶段 00/01。
 * forward：开转发。branch_stall：1 表示阻塞到 MEM 结束，0 表示预测不转移。
 * unified：指令和数据共用一个存储器。freq_mhz：主频，0 表示不换算时间。
 */
typedef struct {
    int forward;
    int branch_stall;
    int unified;
    int diagram;
    uint32_t freq_mhz;
} CpuPipeOpt;

typedef struct {
    int cycles;
    int committed;
    int flushed;
    int ideal;
    int extra;
    int data_stall;
    int ctrl_stall;
    int struct_stall;
} CpuPipeStat;

void cpu_pipe_reset(MmMachine *m);
void cpu_pipe_opt_default(CpuPipeOpt *opt);
void cpu_pipe_set(MmMachine *m, const CpuPipeOpt *opt, int enabled);
int cpu_pipe_enabled(const MmMachine *m);

/* 提交满 max_commit 条，或停机。返回 0 表示已停，返回 1 表示还没停。 */
int cpu_pipe_run(MmMachine *m, int max_commit);

void cpu_pipe_stat(const MmMachine *m, CpuPipeStat *out);

/* seq 从 0 起，按取指顺序，被冲刷的也占号。没有这条返回 -1。 */
int cpu_pipe_id_extra(const MmMachine *m, int seq);
int cpu_pipe_ctrl_of(const MmMachine *m, int seq);

void cpu_pipe_report(const MmMachine *m, FILE *fp);
void cpu_pipe_compare(FILE *fp, uint32_t tick_cycles, uint32_t tick_insns, const MmMachine *m, int same);

#endif
