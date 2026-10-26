#include "flynn.h"

#include <strings.h>

const char *flynn_step(const char *name) {
    if (strcasecmp(name, "sisd") == 0) {
        return "1";
    }
    if (strcasecmp(name, "simd") == 0) {
        return "4";
    }
    if (strcasecmp(name, "misd") == 0) {
        return "无";
    }
    if (strcasecmp(name, "mimd") == 0) {
        return "2";
    }
    return NULL;
}

static void show_sisd(FILE *fp) {
    fputs("【Flynn · SISD】\n", fp);
    fputs("单指令流、单数据流。传统单处理器：一个时刻一条指令，处理一两个数据\n", fp);
    fputs("指令流水线、超标量仍然是 SISD。超标量是同一条指令流里一拍发射多条，没有变成多指令流\n", fp);
    fputs("步骤 ① 的相关链和四个独立累加器都在一个核上，用来看数据相关怎么卡住发射，还是 SISD\n", fp);
    fputs("四个累加器不是 SIMD。SIMD 是一条指令同时处理多个数据，那是步骤 ④\n", fp);
    fputs("结果：SISD\n", fp);
}

static void show_simd(FILE *fp) {
    fputs("【Flynn · SIMD】\n", fp);
    fputs("单指令流、多数据流。一条指令同时作用在多个数据上，是数据级并行\n", fp);
    fputs("步骤 ④ 的 NEON：一条向量加法处理 4 个 float。向量处理器属于 SIMD\n", fp);
    fputs("各通道执行的是同一条运算。遇到各干各的分支时，不满足条件的通道只能空转\n", fp);
    fputs("结果：SIMD\n", fp);
}

static void show_misd(FILE *fp) {
    fputs("【Flynn · MISD】\n", fp);
    fputs("多指令流、单数据流。多条不同的指令去处理同一份数据，实际机器几乎不用\n", fp);
    fputs("本关四步实验都对不上它\n", fp);
    fputs("结果：MISD\n", fp);
}

static void show_mimd(FILE *fp) {
    fputs("【Flynn · MIMD】\n", fp);
    fputs("多指令流、多数据流。多个处理器各取各的指令，各处理各的数据\n", fp);
    fputs("步骤 ② 把同一段计算拆给多个线程，每个核跑自己的那一段。多核处理器属于 MIMD\n", fp);
    fputs("共享同一物理地址空间的是 SMP；各有私有存储器、靠消息传递的是多计算机，也属于 MIMD\n", fp);
    fputs("结果：MIMD\n", fp);
}

int flynn_show(FILE *fp, const char *name) {
    if (strcasecmp(name, "sisd") == 0) {
        show_sisd(fp);
        return 0;
    }
    if (strcasecmp(name, "simd") == 0) {
        show_simd(fp);
        return 0;
    }
    if (strcasecmp(name, "misd") == 0) {
        show_misd(fp);
        return 0;
    }
    if (strcasecmp(name, "mimd") == 0) {
        show_mimd(fp);
        return 0;
    }
    return -1;
}

void flynn_map(FILE *fp) {
    fputs("【Flynn · 和本关实验的对应】\n", fp);
    fputs("SISD：步骤 ①，一个核上的标量累加。数据相关限制的是这条指令流里的发射\n", fp);
    fputs("SIMD：步骤 ④，NEON 一条指令处理多个 float。向量处理器属于 SIMD\n", fp);
    fputs("MISD：没有对应实验\n", fp);
    fputs("MIMD：步骤 ②，多个线程在多个核上各算一段。多核处理器属于 MIMD\n", fp);
    fputs("步骤 ③ 的伪共享不是新的一类，它是 MIMD 下缓存行被两个核来回踢\n", fp);
    fputs("结果：SISD 对应步骤 1，SIMD 对应步骤 4，MISD 无实验，MIMD 对应步骤 2\n", fp);
}
