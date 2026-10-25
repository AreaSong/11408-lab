/*
 * 输入输出比对的演示程序：读入 n 和 n 个整数，输出最大值。
 * 输入格式：第一行 n，第二行 n 个整数
 * 输出格式：max=<最大值>；n 为 0 时输出 empty
 */
#include <stdio.h>
#include <stdlib.h>

#include "max.h"
#include "prompt.h"

int main(void) {
    int n = 0;
    prompt("请输入元素个数 n：");
    if (scanf("%d", &n) != 1 || n < 0) {
        fprintf(stderr, "输入格式错误：第一行应为非负整数 n\n");
        return 1;
    }
    /* n 由输入决定，编译时不知道大小，所以用 malloc 在堆上申请；n 为 0 时不申请 */
    int *a = n > 0 ? malloc(sizeof(int) * (size_t)n) : NULL;
    if (n > 0 && a == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    if (n > 0) {
        prompt("请输入 %d 个整数：", n);
    }
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "输入格式错误：应有 %d 个整数\n", n);
            free(a);
            return 1;
        }
    }

    int m = 0;
    if (max_find(a, n, &m)) {
        printf("max=%d\n", m);
    } else {
        printf("empty\n");
    }
    free(a);
    return 0;
}
