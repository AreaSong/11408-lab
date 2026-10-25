/*
 * 2011-42 的入口：读两个等长升序序列，打印折半过程，最后一行是中位数。
 */
#include <stdio.h>
#include <stdlib.h>

#include "prompt.h"
#include "two_median.h"

static int read_seq(int *a, int n, const char *name) {
    prompt("请输入升序序列 %s 的 %d 个整数：", name, n);
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "输入格式错误：序列 %s 应有 %d 个整数\n", name, n);
            return 0;
        }
    }
    for (int i = 1; i < n; i++) {
        if (a[i] < a[i - 1]) {
            fprintf(stderr, "输入格式错误：序列 %s 必须是升序\n", name);
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int n = 0;
    prompt("请输入序列长度 n：");
    if (scanf("%d", &n) != 1 || n < 1 || n > 100000) {
        fprintf(stderr, "输入格式错误：n 应为 1 到 100000 的整数\n");
        return 1;
    }
    int *a = malloc(sizeof(int) * (size_t)n);
    int *b = malloc(sizeof(int) * (size_t)n);
    if (a == NULL || b == NULL) {
        fprintf(stderr, "内存不足\n");
        free(a);
        free(b);
        return 1;
    }
    int ok = read_seq(a, n, "A") && read_seq(b, n, "B");
    if (ok) {
        printf("median=%d\n", two_median(a, b, n, stdout));
    }
    free(a);
    free(b);
    return ok ? 0 : 1;
}
