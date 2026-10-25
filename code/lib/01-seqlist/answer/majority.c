/*
 * 2013-41 的入口：读一个序列，打印候选抵消过程，最后一行是主元素或 -1。
 */
#include <stdio.h>
#include <stdlib.h>

#include "majority_element.h"
#include "prompt.h"

int main(void) {
    int n = 0;
    prompt("请输入元素个数 n：");
    if (scanf("%d", &n) != 1 || n < 1 || n > 100000) {
        fprintf(stderr, "输入格式错误：n 应为 1 到 100000 的整数\n");
        return 1;
    }
    int *a = malloc(sizeof(int) * (size_t)n);
    if (a == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    prompt("请输入 %d 个整数：", n);
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "输入格式错误：应有 %d 个整数\n", n);
            ok = 0;
            break;
        }
    }
    if (ok) {
        printf("majority=%d\n", majority_element(a, n, stdout));
    }
    free(a);
    return ok ? 0 : 1;
}
