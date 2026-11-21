#include "dev/dev_wire.h"

#include <stdio.h>
#include <string.h>

static int copy_lines(const char *const *src, int n, const char *result, char lines[][128], int cap,
                      char *out, int rcap) {
    int i;
    int keep = n < cap ? n : cap;
    if (lines == NULL || cap < 1 || out == NULL || rcap < 1) {
        return -1;
    }
    for (i = 0; i < keep; i++) {
        snprintf(lines[i], 128, "%s", src[i]);
    }
    snprintf(out, (size_t)rcap, "%s", result);
    return keep;
}

static int wire_sync(char lines[][128], int cap, char *result, int rcap) {
    static const char *text[] = {
        "【同步定时】",
        "主从设备看同一个时钟。事件出现在规定的时钟沿上，没有应答信号",
        "T1 上升沿：主设备把地址送到地址线",
        "T2 上升沿：主设备发出读命令",
        "T2 到 T3：从设备准备数据，必须在 T3 上升沿之前送到数据线",
        "T3：主设备读入数据",
        "T4 上升沿：主设备撤销读命令",
        "同步靠统一时钟和时序余量，不靠握手。各设备各自出一个时钟就对不齐",
    };
    return copy_lines(text, 8, "统一时钟", lines, cap, result, rcap);
}

static int wire_full(char lines[][128], int cap, char *result, int rcap) {
    static const char *text[] = {
        "【异步全互锁】",
        "没有公共时钟。请求和回答互相锁住，撤销前都要等到对方",
        "1. 主设备置请求，然后等待",
        "2. 从设备看到请求，准备好后置回答",
        "3. 主设备看到回答，才撤销请求，然后继续等待",
        "4. 从设备看到请求已经撤销，才撤销回答",
        "5. 主设备看到回答已经撤销，这次握手结束，才能开始下一次",
        "四步都等对方，所以最可靠，也最慢",
    };
    return copy_lines(text, 8, "全互锁", lines, cap, result, rcap);
}

static int wire_half(char lines[][128], int cap, char *result, int rcap) {
    static const char *text[] = {
        "【异步半互锁】",
        "主设备要等到回答，才撤销请求",
        "从设备发出回答后不等请求撤销，过一段时间自己撤销回答",
        "只有请求这一侧互锁",
    };
    return copy_lines(text, 4, "半互锁", lines, cap, result, rcap);
}

static int wire_loose(char lines[][128], int cap, char *result, int rcap) {
    static const char *text[] = {
        "【异步不互锁】",
        "主设备发出请求后不等回答，过一段时间自己撤销",
        "从设备发出回答后也不等，过一段时间自己撤销",
        "两边都不确认对方已经看到信号",
    };
    return copy_lines(text, 4, "不互锁", lines, cap, result, rcap);
}

int dev_wire(const char *kind, char lines[][128], int cap, char *result, int rcap) {
    if (kind == NULL) {
        return -1;
    }
    if (strcmp(kind, "sync") == 0) {
        return wire_sync(lines, cap, result, rcap);
    }
    if (strcmp(kind, "full") == 0) {
        return wire_full(lines, cap, result, rcap);
    }
    if (strcmp(kind, "half") == 0) {
        return wire_half(lines, cap, result, rcap);
    }
    if (strcmp(kind, "loose") == 0) {
        return wire_loose(lines, cap, result, rcap);
    }
    return -1;
}
