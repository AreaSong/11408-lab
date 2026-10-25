/*
 * 观察异常。不进 tests：null 会以信号退出，div0 的结果跟 CPU 有关。
 * 用法：fault div0    或    fault null
 */
#include <stdio.h>
#include <string.h>

#include "arith.h"

static int run_div0(void) {
    volatile int divisor = 0;
    int quotient = divide_int(1, divisor);
    printf("div0=%d\n", quotient);
    return 0;
}

static int run_null(void) {
    volatile int *p = NULL;
    printf("null=%d\n", load_int(p));
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "div0") == 0) {
        return run_div0();
    }
    if (argc == 2 && strcmp(argv[1], "null") == 0) {
        return run_null();
    }
    fprintf(stderr, "用法: fault div0|null\n");
    return 2;
}
