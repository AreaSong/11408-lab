#include <stdio.h>

#include "prompt.h"
#include "max.h"
#include "stdlib.h"
#include "segments.h"
#include "test.h"

int main(void) {
    int n = 0;
    prompt("请输入元素个数 n: ");
    if (scanf("%d", &n) != 1 || n < 0) {
        fprintf(stderr, "输入格式错误：第一行应为非负整数n\n");
        return 1;
    }

    int *a = n > 0 ? malloc(sizeof(int) * (size_t) n) : NULL;
    if (n > 0 && a == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }

    if (n > 0) {
        prompt("请输入 %d 个整数：", n);
        for (int i = 0; i < n; i++) {
            if (scanf("%d", &a[i]) != 1) {
                fprintf(stderr, "请输入格式有误：应有 %d 个整数\n", n);
                free(a);
                return 1;
            }
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
