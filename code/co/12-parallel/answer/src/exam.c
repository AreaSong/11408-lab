#include "exam.h"

#include <string.h>

const char *exam_letter(const char *name) {
    if (strcmp(name, "2022-22") == 0) {
        return "C";
    }
    return NULL;
}

static void show_2022(FILE *fp) {
    fputs("【2022-22】下列关于并行处理技术的叙述中，不正确的是\n", fp);
    fputs("A. 多核处理器属于MIMD结构\n", fp);
    fputs("每个核取自己的指令、处理自己的数据，是多指令流多数据流。叙述正确\n", fp);
    fputs("B. 向量处理器属于SIMD结构\n", fp);
    fputs("一条向量指令同时作用在向量的多个分量上，是单指令流多数据流。叙述正确\n", fp);
    fputs("C. 硬件多线程技术只可用于多核处理器\n", fp);
    fputs("硬件多线程复制的是寄存器组和 PC，不是再做一个核。单核上的超线程就是同时多线程。叙述不正确\n", fp);
    fputs("D. SMP中所有处理器共享单一物理地址空间\n", fp);
    fputs("共享内存多处理器就是各处理器用访存指令访问同一套物理地址。叙述正确\n", fp);
    fputs("结果：答案 C\n", fp);
}

int exam_show(FILE *fp, const char *name) {
    if (strcmp(name, "2022-22") == 0) {
        show_2022(fp);
        return 0;
    }
    return -1;
}
