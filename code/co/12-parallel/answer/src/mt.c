#include "mt.h"

#include <string.h>

static void show_fine(FILE *fp) {
    fputs("【硬件多线程 · 细粒度】\n", fp);
    fputs("每个线程有自己的寄存器组和 PC。ALU、Cache、流水线仍然是这一套，不按线程复制\n", fp);
    fputs("每个时钟周期切换一次，切换只是改用另一套寄存器，不必把现场写回主存\n", fp);
    fputs("周期 1：发射线程 A 的 1 条指令\n", fp);
    fputs("周期 2：发射线程 B 的 1 条指令\n", fp);
    fputs("周期 3：发射线程 A 的 1 条指令\n", fp);
    fputs("周期 4：发射线程 B 的 1 条指令\n", fp);
    fputs("同一时刻只有一个线程在发射。这是时间上交错，不是多核那种同时执行\n", fp);
    fputs("结果：细粒度\n", fp);
}

static void show_coarse(FILE *fp) {
    fputs("【硬件多线程 · 粗粒度】\n", fp);
    fputs("连续执行同一个线程，遇到 Cache 缺失这类长延迟才切换\n", fp);
    fputs("周期 1：发射线程 A\n", fp);
    fputs("周期 2：线程 A 发生 Cache 缺失\n", fp);
    fputs("周期 3：清空 A 的流水线，切换到线程 B 并重新装填\n", fp);
    fputs("周期 4：发射线程 B\n", fp);
    fputs("短的数据相关不切换。一切换就要排空再填满，开销比细粒度大\n", fp);
    fputs("结果：粗粒度\n", fp);
}

static void show_smt(FILE *fp) {
    fputs("【硬件多线程 · 同时多线程 SMT】\n", fp);
    fputs("同一时钟周期从多个线程取指令，填进超标量多出来的发射槽\n", fp);
    fputs("周期 1：同时发射线程 A 的指令和线程 B 的指令\n", fp);
    fputs("周期 2：同时发射线程 A 的指令和线程 B 的指令\n", fp);
    fputs("单发射流水线没有多余的槽，做不了 SMT。它要建立在超标量上\n", fp);
    fputs("第 11 关的超标量是一个线程里一拍多条；SMT 是让多个线程去填那些槽\n", fp);
    fputs("Intel 超线程就是一个核上放两套寄存器和 PC。硬件多线程不要求有多个核\n", fp);
    fputs("结果：同时多线程\n", fp);
}

int mt_show(FILE *fp, const char *kind) {
    if (strcmp(kind, "fine") == 0) {
        show_fine(fp);
        return 0;
    }
    if (strcmp(kind, "coarse") == 0) {
        show_coarse(fp);
        return 0;
    }
    if (strcmp(kind, "smt") == 0) {
        show_smt(fp);
        return 0;
    }
    return -1;
}
