#include "pipe.h"

#include <stdio.h>
#include <string.h>

static const char *cell_name(char cell) {
    switch (cell) {
    case 'F':
        return "IF";
    case 'D':
        return "ID";
    case 'X':
        return "EX";
    case 'M':
        return "MEM";
    case 'W':
        return "WB";
    default:
        return "";
    }
}

static int earlier_writer(const Insn *ins, int i, int reg) {
    int j;
    for (j = i - 1; j >= 0; j--) {
        if (ins[j].rd == reg) {
            return j;
        }
    }
    return -1;
}

static void print_mode(FILE *fp, const PipeOpt *opt) {
    fputs("【五段流水线", fp);
    if (opt->forward) {
        fputs(" · 有转发", fp);
    } else if (opt->same_cycle) {
        fputs(" · 无转发 · 先写后读", fp);
    } else {
        fputs(" · 无转发 · 同拍不能读写", fp);
    }
    fputs(opt->unified ? " · 存储器不分开" : " · 指令和数据存储器分开", fp);
    if (opt->branch_stall) {
        fputs(" · 分支等到访存段结束", fp);
    }
    if (opt->predict) {
        fputs(" · 预测不转移", fp);
    }
    if (opt->delay_slot) {
        fputs(" · 延迟槽", fp);
    }
    fputs("】\n", fp);
    fputs("约定：IF 取指，ID 译码并读寄存器，EX 执行或算地址，MEM 访存，WB 写回。\n", fp);
}

static void print_raw(FILE *fp, const Insn *ins, int n, const PipeOpt *opt) {
    int i;
    int k;
    for (i = 1; i < n; i++) {
        for (k = 0; k < ins[i].nex; k++) {
            int p = earlier_writer(ins, i, ins[i].ex[k]);
            if (p >= 0) {
                fprintf(fp, "相关：I%d 读 %s，I%d 写 %s（RAW）\n", i + 1, ins[i].ex_name[k], p + 1,
                        ins[i].ex_name[k]);
            }
        }
        for (k = 0; k < ins[i].nmem; k++) {
            int p = earlier_writer(ins, i, ins[i].memsrc[k]);
            if (p >= 0) {
                fprintf(fp, "相关：I%d 读 %s，I%d 写 %s（RAW%s）\n", i + 1, ins[i].mem_name, p + 1,
                        ins[i].mem_name, opt->forward ? "，数在 MEM 段才用" : "");
            }
        }
    }
}

static void print_stalls(FILE *fp, const Insn *ins, int n, const PipeOpt *opt, const PipeStat *st) {
    int i;
    int any = 0;
    for (i = 0; i < n; i++) {
        if (!st->flushed[i]) {
            continue;
        }
        if (any) {
            fputc(' ', fp);
        } else {
            fputs("冲刷：", fp);
        }
        fprintf(fp, "I%d", i + 1);
        any = 1;
    }
    if (any) {
        fputs("（预测不转移，实际转移，EX 段才知道，IF 和 ID 里的指令作废）\n", fp);
    }
    for (i = 0; i < n; i++) {
        if (st->if_first[i] == 0) {
            fprintf(fp, "I%d 没有进入流水线\n", i + 1);
        }
        if (st->id_extra[i] > 0) {
            fprintf(fp, "I%d 数据阻塞 %d 拍（停在 ID）\n", i + 1, st->id_extra[i]);
        }
        if (st->if_extra[i] > 0) {
            fprintf(fp, "I%d 取指堵住 %d 拍（前一条还占着 ID，按序不能越过）\n", i + 1,
                    st->if_extra[i]);
        }
        if (st->ctrl[i] > 0) {
            fprintf(fp, "I%d 控制阻塞 %d 拍（分支的 MEM 结束之后才取指）\n", i + 1, st->ctrl[i]);
        }
    }
    if (opt->delay_slot) {
        for (i = 0; i + 1 < n; i++) {
            if (ins[i].is_branch && st->if_first[i + 1] > 0 && !st->flushed[i + 1]) {
                fprintf(fp, "I%d 在延迟槽里，转移与否都执行\n", i + 2);
            }
        }
    }
}

static void print_chart(FILE *fp, int n, const PipeStat *st) {
    int c;
    int i;
    fputs("时空图\n时钟", fp);
    for (c = 1; c <= st->cycles; c++) {
        fprintf(fp, "%5d", c);
    }
    fputc('\n', fp);
    for (i = 0; i < n; i++) {
        fprintf(fp, "I%-3d", i + 1);
        for (c = 1; c <= st->cycles; c++) {
            fprintf(fp, "%5s", cell_name(st->cell[i][c]));
        }
        fputc('\n', fp);
    }
}

static int sum_arr(const int *a, int n) {
    int i;
    int s = 0;
    for (i = 0; i < n; i++) {
        s += a[i];
    }
    return s;
}

static void print_summary(FILE *fp, int n, const PipeOpt *opt, const PipeStat *st) {
    int rest = st->extra - sum_arr(st->id_extra, n) - sum_arr(st->ctrl, n);
    fprintf(fp, "提交 %d 条，理想 %d 个时钟周期，实际 %d 个时钟周期\n", st->committed, st->ideal,
            st->cycles);
    if (opt->unified && rest > 0) {
        fprintf(fp, "结构阻塞 %d 拍（取指和数据访存抢同一个存储器）\n", rest);
    }
    if (opt->predict) {
        fprintf(fp, "结果：冲刷 %d 条，惩罚 %d 拍，共 %d 个时钟周期\n", st->nflush, st->extra,
                st->cycles);
        return;
    }
    if (opt->delay_slot) {
        fprintf(fp, "结果：延迟槽惩罚 %d 拍，共 %d 个时钟周期\n", st->extra, st->cycles);
        return;
    }
    fprintf(fp, "结果：阻塞 %d 拍，共 %d 个时钟周期\n", st->extra, st->cycles);
}

void pipe_report(FILE *fp, const Insn *ins, int n, const PipeOpt *opt, const PipeStat *st) {
    int i;
    print_mode(fp, opt);
    for (i = 0; i < n; i++) {
        fprintf(fp, "I%d  %s\n", i + 1, ins[i].text);
    }
    print_raw(fp, ins, n, opt);
    print_stalls(fp, ins, n, opt, st);
    print_chart(fp, n, st);
    print_summary(fp, n, opt, st);
}
