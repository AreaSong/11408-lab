#include "arith.h"

/*
 * 整数除零在 C 里是未定义行为。调用方用 volatile 传入 0，本文件又看不到那个 0，
 * 于是编译器只能留下除法指令，结果由 CPU 决定。
 * arm64 的整数除零得到 0，且不陷入内核；考纲按会陷入的机器（如 x86）把除零当成异常。
 */
int divide_int(int dividend, int divisor) {
    return dividend / divisor;
}

/* 对空指针取值会变成一次非法访存，由硬件报异常，进程被终止。 */
int load_int(volatile int *p) {
    return *p;
}
