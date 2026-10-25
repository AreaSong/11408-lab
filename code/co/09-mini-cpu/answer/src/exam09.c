#include "exam.h"

#include "beat.h"

#include <stdio.h>

/* 2009 年第 44 题：取指四拍题干已给，执行阶段有两种都不冲突的排法。 */
static const Micro FETCH09[] = {
    {"取指", "(PC)->MAR", "题干已给出", "PCout MARin"},
    {"取指", "M(MAR)->MDR (PC)+1->PC", "PC+1 不走内总线", "MemR MDRinE PC+1"},
    {"取指", "(MDR)->IR", "这一拍取出的是指令", "MDRout IRin"},
    {"译码", "OP(IR)->CU", "译码不占用内总线", "无"},
};

static const Micro EXEC6[] = {
    {"访存", "(R1)->MAR", "先送间接地址", "R1out MARin"},
    {"访存", "M(MAR)->MDR", "这一拍取出的是数据", "MemR MDRinE"},
    {"执行", "(MDR)->A", "先锁存主存操作数", "MDRout Ain"},
    {"执行", "(A)+(R0)->AC", "R0 单独上内总线", "R0out Add ACin"},
    {"写回", "(AC)->MDR", "结果离开 ALU", "ACout MDRin"},
    {"访存", "(MDR)->M(MAR)", "MAR 仍是 (R1)，写回主存", "MDRoutE MemW"},
};

static const Micro EXEC5[] = {
    {"访存", "(R1)->MAR", "先送间接地址", "R1out MARin"},
    {"访存", "M(MAR)->MDR (R0)->A", "数据总线与内总线同时", "MemR MDRinE R0out Ain"},
    {"执行", "(MDR)+(A)->AC", "A 里已经是 R0", "MDRout Add ACin"},
    {"写回", "(AC)->MDR", "结果离开 ALU", "ACout MDRin"},
    {"访存", "(MDR)->M(MAR)", "MAR 仍是 (R1)，写回主存", "MDRoutE MemW"},
};

static int print_plan(FILE *out, const char *title, const Micro *list, int n) {
    int bad = micro_print(out, list, n, 5);
    fprintf(out, "%s：%d 个时钟周期，冲突 %d 次\n", title, n, bad);
    return bad;
}

int show_2009(FILE *out) {
    int bad6;
    int bad5;
    int clash = driver_count("R1out R0out MARin");
    fprintf(out, "【2009-44】ADD (R1), R0\n");
    fprintf(out, "功能：(R0)+((R1)) 写入 (R1) 所指主存，不写回寄存器 R1\n");
    fprintf(out, "取指和译码 4 个节拍\n");
    micro_print(out, FETCH09, 4, 1);
    fprintf(out, "执行阶段方案一，主存操作数先单独锁进 A\n");
    bad6 = print_plan(out, "方案一", EXEC6, 6);
    fprintf(out, "执行阶段方案二，读主存的同时把 R0 送进 A\n");
    bad5 = print_plan(out, "方案二", EXEC5, 5);
    fprintf(out, "反例：同一拍 R1out 和 R0out，内总线驱动 %d 个，冲突\n", clash);
    fprintf(out, "结果：6 个时钟周期或 5 个时钟周期\n");
    return (bad6 || bad5 || clash != 2) ? 1 : 0;
}
