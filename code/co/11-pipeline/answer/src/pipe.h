#ifndef PIPE_PIPE_H
#define PIPE_PIPE_H

#include "insn.h"

#include <stdio.h>

enum { PIPE_C = 64 };

/*
 * 408 的五段模型：IF、ID、EX、MEM、WB。
 * same_cycle = 1：WB 前半写、ID 后半读，同一拍可以读到刚写回的值（相邻 RAW 停 2 拍）。
 * same_cycle = 0：题目写明同一拍不能又读又写（2012-44，相邻 RAW 停 3 拍）。
 * forward：ALU 结果从 EX 末转发；load 的数要到 MEM 末才有，下一条在 EX 要用就再停 1 拍。
 * branch_stall：分支要等自己的 MEM 段结束才取下一条，惩罚 3 拍（2023、2014）。
 * predict：预测不转移，实际转移时在 EX 段末冲刷 IF 和 ID 里的两条。
 * delay_slot：紧挨着分支的那条总是执行，分支在 ID 段末就能改 PC。
 */

typedef struct PipeOpt {
    int forward;
    int same_cycle;
    int unified;
    int branch_stall;
    int predict;
    int delay_slot;
} PipeOpt;

typedef struct PipeStat {
    int cycles;
    int ideal;
    int extra;
    int committed;
    int nflush;
    int id_extra[INSN_N];
    int if_extra[INSN_N];
    int ctrl[INSN_N];
    int if_first[INSN_N];
    int flushed[INSN_N];
    char cell[INSN_N][PIPE_C];
} PipeStat;

int pipe_opt_named(const char *name, PipeOpt *opt);
int pipe_run(const Insn *ins, int n, const PipeOpt *opt, PipeStat *st);
void pipe_report(FILE *fp, const Insn *ins, int n, const PipeOpt *opt,
                 const PipeStat *st);

#endif
