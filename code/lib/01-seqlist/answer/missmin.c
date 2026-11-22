/*
 * 2018-41 的入口。每组先读 n，再读 n 个整数，读到输入结束。
 * 打印未出现的最小正整数。过程不展开标记数组，单元测试直接查返回值。
 */
#include <stdio.h>
#include <stdlib.h>

#include "missmin.h"
#include "prompt.h"

static int run_case(int n) {
    int *a = malloc(sizeof(int) * (size_t)n);
    int value;
    if (a == NULL) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    prompt("请输入 %d 个整数：", n);
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "输入格式错误：应有 %d 个整数\n", n);
            free(a);
            return 0;
        }
    }
    value = find_miss_min(a, n);
    if (value < 0) {
        fprintf(stderr, "内存不足\n");
        free(a);
        return 0;
    }
    printf("miss=%d\n", value);
    free(a);
    return 1;
}

int main(void) {
    for (;;) {
        int n = 0;
        int got;
        prompt("请输入 n（Ctrl+D 结束）：");
        got = scanf("%d", &n);
        if (got == EOF) {
            break;
        }
        if (got != 1 || n < 1 || n > 100000) {
            fprintf(stderr, "输入格式错误：n 应为 1 到 100000 的整数\n");
            return 1;
        }
        if (!run_case(n)) {
            return 1;
        }
    }
    return 0;
}
