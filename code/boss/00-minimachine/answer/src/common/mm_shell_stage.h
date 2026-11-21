#ifndef MM_SHELL_STAGE_H
#define MM_SHELL_STAGE_H

#include "common/mm_machine.h"

#include <stdio.h>

typedef struct {
    int kernel;
    int user;
    int ei;
    int event;
    int imr_set;
    uint32_t imr;
    int mask_set[16];
    uint32_t mask[16];
    int steps_set[16];
    int steps[16];
    int raise[16];
    int nmi;
    int timer_set;
    uint32_t timer;
    char conin[160];
    int pipe;
    int forward;
    int branch_stall;
    int unified;
    int diagram;
    uint32_t freq_mhz;
    int l1_on;
    int l1_lines;
    int l1_ways;
    int l1_block;
    int l1_replace;
    int l1_wb;
    int l1_alloc;
    int l1_mem_cycles;
    int l1_split;
    uint32_t l1_seed;
    int pg;
    int tlb_entries;
    int tlb_ways;
    int tlb_repl;
    int walk_cycles;
    uint32_t ptbr;
    int map_n;
    uint32_t map_va[16];
    uint32_t map_pa[16];
    uint8_t map_w[16];
    uint8_t map_u[16];
    int plant_n;
    uint8_t plant_set[8];
    uint8_t plant_way[8];
    uint8_t plant_valid[8];
    uint32_t plant_tag[8];
    uint32_t plant_frame[8];
    uint8_t plant_w[8];
    uint8_t plant_u[8];
    int fix_on;
    uint32_t fix_va;
    uint32_t fix_pa;
    int proc;
    int demand;
    int demand_frames;
    char demand_algo[8];
    int fs;
} MmShellOpt;

void mm_shell_opt_init(MmShellOpt *opt);
void mm_shell_apply(MmMachine *m, const MmShellOpt *opt);
void mm_shell_bind(MmMachine *m, FILE *out, const MmShellOpt *opt, int trace_on);
/* 返回 1 表示这条命令属于阶段 01，0 表示不是，-1 表示命令写错了。 */
int mm_shell_stage(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt);

/* 阶段 02 的流水线命令。返回值同 mm_shell_stage。 */
int mm_shell_pipe(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt,
                  AsmImage *image);

/* 阶段 03 的主存和 Cache 命令。返回值同 mm_shell_stage。 */
int mm_shell_mem(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt);
void mm_shell_mem_apply(MmMachine *m, const MmShellOpt *opt);

/* 阶段 04 的页表和 TLB 命令。返回值同 mm_shell_stage。 */
int mm_shell_calc(const char *line, FILE *out);
int mm_shell_mmu(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt);
void mm_shell_mmu_apply(MmMachine *m, const MmShellOpt *opt);

/* 阶段 05 的进程和调度命令。返回值同 mm_shell_stage。sched 会继续读 in，直到 end。 */
int mm_shell_proc(char *line, FILE *in, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt,
                  struct AsmImage **image);

/* 阶段 06 的信号量、银行家和死锁命令。banker、reduce、recover、sem 会继续读 in，直到 end。 */
int mm_shell_sync(char *line, FILE *in, FILE *out, MmMachine **machine);

/* 阶段 07 的置换、伙伴系统、缺页和 sbrk/mmap。返回值同 mm_shell_stage。 */
int mm_shell_vm(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt);

/* 阶段 08 的磁盘、文件系统命令。返回值同 mm_shell_stage。 */
int mm_shell_fs(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt);

#endif
