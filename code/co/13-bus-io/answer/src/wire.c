#include "wire.h"

#include <stdio.h>
#include <string.h>

#include "prompt.h"

static void print_lines(const char *const *lines, int n) {
    int i;
    for (i = 0; i < n; i++) {
        printf("%s\n", lines[i]);
    }
}

static int print_sync(void) {
    static const char *lines[] = {
        "【同步定时】",
        "主从设备看同一个时钟。事件出现在规定的时钟沿上，没有应答信号",
        "下面是一次读：CPU 是主设备，输入设备是从设备",
        "T1 上升沿：主设备把地址送到地址线",
        "T2 上升沿：主设备发出读命令",
        "T2 到 T3：从设备准备数据，必须在 T3 上升沿之前送到数据线",
        "T3：主设备读入数据",
        "T4 上升沿：主设备撤销读命令，从设备停止驱动数据线",
        "时钟由系统统一提供。各设备各自出一个时钟就对不齐",
        "从设备来不及，就在规定的沿上读到错数据。同步靠时序余量，不靠握手",
        "结果：统一时钟",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

static int print_full(void) {
    static const char *lines[] = {
        "【异步全互锁】",
        "没有公共时钟。请求和回答互相锁住，撤销前都要等到对方",
        "1. 主设备置请求，然后等待",
        "2. 从设备看到请求，准备好后置回答",
        "3. 主设备看到回答，才撤销请求，然后继续等待",
        "4. 从设备看到请求已经撤销，才撤销回答",
        "5. 主设备看到回答已经撤销，这次握手结束，才能开始下一次",
        "四步都等对方，所以最可靠，也最慢",
        "结果：全互锁",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

static int print_half(void) {
    static const char *lines[] = {
        "【异步半互锁】",
        "主设备要等到回答，才撤销请求",
        "从设备发出回答后不等请求撤销，过一段时间自己撤销回答",
        "只有请求这一侧互锁",
        "结果：半互锁",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

static int print_loose(void) {
    static const char *lines[] = {
        "【异步不互锁】",
        "主设备发出请求后不等回答，过一段时间自己撤销",
        "从设备发出回答后也不等，过一段时间自己撤销",
        "两边都不确认对方已经看到信号，所以最快，也最容易把没送到的信号当成已经完成",
        "结果：不互锁",
    };
    print_lines(lines, (int)(sizeof lines / sizeof lines[0]));
    return 0;
}

int wire_command(const char *cmd) {
    char mode[16];
    if (strcmp(cmd, "wire") != 0) {
        return -1;
    }
    prompt("请输入 sync、full、half 或 loose：");
    if (scanf("%15s", mode) != 1) {
        fprintf(stderr, "wire 需要 sync、full、half 或 loose\n");
        return 1;
    }
    if (strcmp(mode, "sync") == 0) {
        return print_sync();
    }
    if (strcmp(mode, "full") == 0) {
        return print_full();
    }
    if (strcmp(mode, "half") == 0) {
        return print_half();
    }
    if (strcmp(mode, "loose") == 0) {
        return print_loose();
    }
    fprintf(stderr, "wire 只能是 sync、full、half 或 loose\n");
    return 1;
}
