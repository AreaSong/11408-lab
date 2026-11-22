#include "pathcmp.h"

/*
 * 同一条 add：单总线必须把一个操作数先送进暂存器。
 * 三总线让 ALU 的两个输入和结果各走一条，专用通路把取指里不冲突的传送并到同一拍。
 */

int path_single_beats(void) {
    return 8;
}

int path_triple_beats(void) {
    return 5;
}

static void line(FILE *out, const char *kind, int index, const char *stage, const char *func,
                 const char *how) {
    fprintf(out, "%s C%d %s | %s | %s\n", kind, index, stage, func, how);
}

static void show_single(FILE *out) {
    fprintf(out, "单总线：内总线只有一条。ALU 两个输入不能同时到达，先把主存的数送进暂存器 A\n");
    line(out, "单总线", 1, "取指", "(PC)->MAR", "PCout MARin");
    line(out, "单总线", 2, "取指", "M(MAR)->MDR (PC)+1->PC", "MemR MDRinE PC+1");
    line(out, "单总线", 3, "取指", "(MDR)->IR", "MDRout IRin");
    line(out, "单总线", 4, "译码", "OP(IR)->CU", "无");
    line(out, "单总线", 5, "执行", "(IR的地址)->MAR", "IRout MARin");
    line(out, "单总线", 6, "访存", "M(MAR)->MDR", "MemR MDRinE");
    line(out, "单总线", 7, "执行", "(MDR)->A", "MDRout Ain");
    line(out, "单总线", 8, "写回", "(A)+(ACC)->ACC", "ACCout Add ACin");
    fprintf(out, "单总线拍数=%d\n", path_single_beats());
}

static void show_triple(FILE *out) {
    fprintf(out, "三总线：ALU 两输入和输出各一条。PC 到 MAR、主存到 IR 走专用通路，不再为每次传送单独排队\n");
    line(out, "三总线", 1, "取指", "(PC)->MAR (PC)+1->PC", "专用通路");
    line(out, "三总线", 2, "取指", "M(MAR)->IR 并译码", "专用通路");
    line(out, "三总线", 3, "执行", "(IR的地址)->MAR", "总线C");
    line(out, "三总线", 4, "访存", "M(MAR)->MDR", "数据总线");
    line(out, "三总线", 5, "写回", "(MDR)+(ACC)->ACC", "总线A=MDR 总线B=ACC 总线C=结果");
    fprintf(out, "三总线拍数=%d\n", path_triple_beats());
}

static void show_card(FILE *out) {
    fprintf(out, "结构 单总线 | 部件都挂在一条内总线上 | 同一拍只有一个输出 | 需要暂存器 | 节拍最多\n");
    fprintf(out, "结构 双总线 | 两条内总线 | 可以一边送操作数一边送结果，两个操作数仍要分拍 | 通常仍有暂存器 | 介于两者之间\n");
    fprintf(out, "结构 三总线 | ALU 两输入和输出各一条 | 两个操作数和结果可以同一拍 | 可以不要暂存器 | 节拍最少\n");
    fprintf(out, "结构 专用通路 | 固定部件之间直接连线，例如 PC 到 MAR | 这些传送不占用内总线 | 取指里不冲突的传送可以同拍 | 用在取指\n");
}

int show_path(FILE *out) {
    int saved = path_single_beats() - path_triple_beats();
    fprintf(out, "【数据通路对比】指令 add\n");
    show_single(out);
    show_triple(out);
    fprintf(out, "结果：单总线 %d 拍，三总线 %d 拍，少 %d 拍\n", path_single_beats(), path_triple_beats(),
            saved);
    show_card(out);
    return 0;
}
