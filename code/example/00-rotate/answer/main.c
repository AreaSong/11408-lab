/*
 * 输入格式：第一行 n p（1 <= n，0 <= p < n），第二行 n 个整数
 * 输出格式：左移 p 位后的 n 个整数，空格分隔
 */
#include <stdio.h>
#include <stdlib.h>

#include "prompt.h"
#include "rotate.h"

int main(void) {
    int n = 0, p = 0;
    prompt("请输入元素个数 n 和左移位数 p：");
    if (scanf("%d %d", &n, &p) != 2 || n < 1 || p < 0 || p >= n) {
        fprintf(stderr, "输入格式错误：第一行应为 n p，且 1 <= n、0 <= p < n\n");
        return 1;
    }
    int *a = malloc(sizeof(int) * (size_t)n);
    if (a == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    prompt("请输入 %d 个整数：", n);
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "输入格式错误：应有 %d 个整数\n", n);
            free(a);
            return 1;
        }
    }

    rotate_left(a, n, p);

    for (int i = 0; i < n; i++) {
        printf(i == 0 ? "%d" : " %d", a[i]);
    }
    printf("\n");
    free(a);
    return 0;
}
