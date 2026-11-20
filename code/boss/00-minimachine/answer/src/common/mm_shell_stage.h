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
} MmShellOpt;

void mm_shell_opt_init(MmShellOpt *opt);
void mm_shell_apply(MmMachine *m, const MmShellOpt *opt);
void mm_shell_bind(MmMachine *m, FILE *out, const MmShellOpt *opt, int trace_on);
/* 返回 1 表示这条命令属于阶段 01，0 表示不是，-1 表示命令写错了。 */
int mm_shell_stage(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt);

/* 阶段 02 的流水线命令。返回值同 mm_shell_stage。 */
int mm_shell_pipe(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt,
                  AsmImage *image);

#endif
