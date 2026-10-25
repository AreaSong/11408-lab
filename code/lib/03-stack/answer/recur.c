/*
 * 用顺序栈把阶乘递归摊开，打印每一次调用和返回时的栈深。
 * 每行一个 n，0 ≤ n ≤ 20。
 */
#include <stdio.h>

#include "prompt.h"
#include "recur.h"

int main(void) {
    for (;;) {
        int n = 0;
        int got = 0;
        FactResult result;
        prompt("请输入 n（Ctrl+D 结束）：");
        got = scanf("%d", &n);
        if (got == EOF) {
            return 0;
        }
        if (got != 1) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
        printf("case n=%d\n", n);
        if (!fact_by_stack(n, stdout, &result)) {
            printf("fail n=%d\n", n);
        }
    }
}
