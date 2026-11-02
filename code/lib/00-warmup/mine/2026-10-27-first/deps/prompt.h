#ifndef PROMPT_H
#define PROMPT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * 输入前的提示，用法和 printf 一样：prompt("请输入 %d 个整数：", n);
 * 测试脚本运行程序时会设置环境变量 LAB_TEST，这时不打印提示，输出才能和 .out 逐字比对；
 * 自己在 CLion 里运行时没有这个变量，提示正常显示。
 */
static inline void prompt(const char *fmt, ...) {
    if (getenv("LAB_TEST") != NULL) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);   /* 提示不带换行，不刷新的话会停在缓冲区里，等输入完才显示 */
}

#endif
