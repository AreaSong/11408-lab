#include "flow.h"

#include <stdio.h>
#include <string.h>

#include "prompt.h"

static void print_lines(const char *const *lines, int n) {
    int i;
    for (i = 0; i < n; i++) {
        printf("%s\n", lines[i]);
    }
}

static int print_multi(void) {
    static const char *lines[] = {
        "【中断响应与中断处理】多重中断",
        "外部中断在每条指令执行结束时才采样，不把当前指令从中途切断",
        "中断响应由硬件做完，这组操作叫中断隐指令，它本身不是一条指令",
        "1. 关中断，避免保存断点的过程再被打断",
        "2. 保存断点：PC 和程序状态字 PSW",
        "3. 识别中断源，把服务程序入口送入 PC",
        "通用寄存器不在隐指令里保存",
        "中断处理是服务程序，由软件做：",
        "1. 保护现场（通用寄存器）。这一步仍在关中断下做完",
        "2. 送入本中断源的屏蔽字",
        "3. 开中断。屏蔽字里为 0 的请求才能嵌套",
        "4. 执行具体服务",
        "5. 关中断，恢复现场不能被打断",
        "6. 恢复现场",
        "7. 开中断，并执行中断返回。返回时硬件恢复断点和 PSW",
        "结果：响应是隐指令，处理是服务程序",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

static int print_single(void) {
    static const char *lines[] = {
        "【中断响应与中断处理】单重中断",
        "隐指令同样是关中断、保存 PC 和 PSW、把入口送入 PC",
        "服务程序保护现场、做服务、恢复现场，期间不再开中断",
        "所以服务程序不会被另一个中断嵌套",
        "中断返回时硬件恢复断点和 PSW，并回到开中断",
        "结果：服务期间关中断",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

static int print_dma(void) {
    static const char *lines[] = {
        "【DMA 传送】",
        "预处理（CPU）：送传送方向、主存首地址、字数，然后启动设备",
        "数据传送（DMA 控制器）：请求总线，传一个字，地址加 1、计数减 1，直到计数为 0",
        "这条通路在设备接口和主存之间，数据不进入 CPU",
        "后处理（CPU）：传送结束时 DMA 发中断，CPU 做校验，决定结束还是再送一块",
        "和 CPU 同时要总线时有三种做法：",
        "停止 CPU 访存：一整段传送都交给 DMA，CPU 停在访存之外",
        "周期挪用：每次只挪用一个存取周期；两边同时要时 DMA 优先，否则数据会丢",
        "交替访问：总线周期一分为二，两边各用一半，没有冲突",
        "结果：CPU 只做预处理和后处理",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

int flow_command(const char *cmd) {
    char mode[16];
    if (strcmp(cmd, "flow") != 0) {
        return -1;
    }
    prompt("请输入 multi、single 或 dma：");
    if (scanf("%15s", mode) != 1) {
        fprintf(stderr, "flow 需要 multi、single 或 dma\n");
        return 1;
    }
    if (strcmp(mode, "multi") == 0) {
        return print_multi();
    }
    if (strcmp(mode, "single") == 0) {
        return print_single();
    }
    if (strcmp(mode, "dma") == 0) {
        return print_dma();
    }
    fprintf(stderr, "flow 只能是 multi、single 或 dma\n");
    return 1;
}
