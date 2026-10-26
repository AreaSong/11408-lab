#include "exam.h"

#include "calc.h"

#include <string.h>

static void exam_2009_17(FILE *out) {
    fprintf(out, "【2009-17】RISC 与控制器\n");
    fprintf(out, "RISC 指令少、格式固定，多数指令一个时钟周期能做完，靠通用寄存器减少访存\n");
    fprintf(out, "速度来自硬布线。普遍采用微程序控制器的是 CISC\n");
    fprintf(out, "结果：答案 A\n");
}

static void exam_2009_19(FILE *out) {
    fprintf(out, "【2009-19】硬布线相对于微程序\n");
    fprintf(out, "硬布线是组合逻辑，延迟小，改指令功能要改电路\n");
    fprintf(out, "微程序把微命令放进控存，改微程序就能扩展，但每条微指令都要读控存\n");
    fprintf(out, "结果：答案 D\n");
}

static void exam_2012_18(FILE *out) {
    static const int groups[] = {7, 3, 12, 5, 6};
    fprintf(out, "【2012-18】字段直接编码的操作控制字段\n");
    print_groups(out, groups, 5, NULL, NULL);
    fprintf(out, "这五组都不是 2 的幂，留不留不操作，位数都是 15 位\n");
    fprintf(out, "留出一个不发微命令的状态仍是字段直接编码的规则\n");
    fprintf(out, "A 的 5 位是互斥组的组数\n");
    fprintf(out, "B 的 6 位是把 33 个微命令合成一个操作码：2^5 = 32 不够，2^6 = 64 够\n");
    fprintf(out, "那是垂直型，一条微指令只能发出一个微命令\n");
    fprintf(out, "D 的 33 位是直接编码，一位一个微命令\n");
    fprintf(out, "结果：答案 C\n");
}

static void exam_2012_22(FILE *out) {
    fprintf(out, "【2012-22】中断隐指令\n");
    fprintf(out, "隐指令由硬件在响应时完成：关中断，保护断点，把入口地址送入 PC\n");
    fprintf(out, "保存通用寄存器是中断服务程序里的工作，不是隐指令\n");
    fprintf(out, "结果：答案 B\n");
}

static void exam_2014_18(FILE *out) {
    fprintf(out, "【2014-18】断定法的下地址字段\n");
    print_next_body(out, 32, 2, 4, NULL);
    fprintf(out, "A 的 5 位只能区分 32 条机器指令\n");
    fprintf(out, "B 的 6 位只算了 32 条机器指令加 2 条公共微指令，共 34，没有乘每条的 4 条微指令\n");
    fprintf(out, "2^5 = 32 不够表示 34，于是写成 6 位。2^6 = 64 条，仍然装不下 130 条微指令\n");
    fprintf(out, "D 的 9 位够用，但 8 位已经够，不是至少\n");
    fprintf(out, "结果：答案 C\n");
}

static void exam_2015_22(FILE *out) {
    fprintf(out, "【2015-22】故障、陷阱、终止\n");
    fprintf(out, "内部异常与当前指令相关，由 CPU 内部检测，响应发生在指令执行过程中\n");
    fprintf(out, "故障返回本条并重新执行，陷阱返回下一条，终止不返回\n");
    fprintf(out, "D 把三类都说成返回本条，所以错\n");
    fprintf(out, "结果：答案 D\n");
}

static void exam_2015_24(FILE *out) {
    fprintf(out, "【2015-24】哪条指令不会进入内核态\n");
    fprintf(out, "本题在试卷的操作系统部分。DIV 可能除零，INT 是陷阱，MOV 访存可能缺页\n");
    fprintf(out, "NOT 只改寄存器，不会因此从用户态进入内核态\n");
    fprintf(out, "结果：答案 C\n");
}

static void exam_2023_21(FILE *out) {
    fprintf(out, "【2023-21】异常和中断的检测\n");
    fprintf(out, "异常在指令执行过程中检测。外部中断在指令执行结束时检测\n");
    fprintf(out, "开中断时，这次检测发现请求就进入中断响应\n");
    fprintf(out, "请求若在指令中途到达，要等到本条结束才采样，不会把指令打断\n");
    fprintf(out, "中断结束信号由 CPU 发给中断控制器。设备发给控制器的是中断请求\n");
    fprintf(out, "结果：答案 D\n");
}

static void exam_2024_13(FILE *out) {
    fprintf(out, "【2024-13】CPU 直接执行的是什么\n");
    fprintf(out, "机器指令是软件和 CPU 的接口。微指令存在控存里，由控制器直接发出\n");
    fprintf(out, "汇编指令要汇编成机器指令。伪指令是汇编器识别的指示，不是机器指令\n");
    fprintf(out, "结果：答案 B\n");
}

static void exam_2025_16(FILE *out) {
    fprintf(out, "【2025-16】ISA 规定什么\n");
    fprintf(out, "定长指令字属于指令集，软件和编译器要知道\n");
    fprintf(out, "微程序控制器、单总线、阵列乘法器都是同一种指令集可以换的实现\n");
    fprintf(out, "结果：答案 B\n");
}

static void exam_2025_22(FILE *out) {
    fprintf(out, "【2025-22】谁会发外部中断\n");
    fprintf(out, "DMA 传送结束时，DMA 控制器向 CPU 发中断请求，这是外部中断\n");
    fprintf(out, "页故障是故障，断点指令是陷阱，都是内部异常\n");
    fprintf(out, "一次总线事务结束只是这次传输完成，本身不构成中断请求\n");
    fprintf(out, "结果：答案 A\n");
}

int show_exam(FILE *out, const char *id) {
    if (strcmp(id, "2009-17") == 0) { exam_2009_17(out); return 0; }
    if (strcmp(id, "2009-19") == 0) { exam_2009_19(out); return 0; }
    if (strcmp(id, "2012-18") == 0) { exam_2012_18(out); return 0; }
    if (strcmp(id, "2012-22") == 0) { exam_2012_22(out); return 0; }
    if (strcmp(id, "2014-18") == 0) { exam_2014_18(out); return 0; }
    if (strcmp(id, "2015-22") == 0) { exam_2015_22(out); return 0; }
    if (strcmp(id, "2015-24") == 0) { exam_2015_24(out); return 0; }
    if (strcmp(id, "2023-21") == 0) { exam_2023_21(out); return 0; }
    if (strcmp(id, "2024-13") == 0) { exam_2024_13(out); return 0; }
    if (strcmp(id, "2025-16") == 0) { exam_2025_16(out); return 0; }
    if (strcmp(id, "2025-22") == 0) { exam_2025_22(out); return 0; }
    return -1;
}
