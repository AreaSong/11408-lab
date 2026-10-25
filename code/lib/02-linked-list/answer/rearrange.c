/*
 * 2019-41 的入口。每组第一行是 n，接着 n 个整数，读到输入结束。
 * n = 0 时没有后续整数，表示只有头结点的空表。
 */
#include <stdio.h>

#include "prompt.h"
#include "rearrange.h"
#include "slist.h"

static int run_case(int n) {
    SList list;
    if (!slist_init(&list)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    if (n > 0) {
        prompt("请输入 a1 到 a%d：", n);
    }
    for (int i = 0; i < n; i++) {
        int value = 0;
        if (scanf("%d", &value) != 1) {
            fprintf(stderr, "输入格式错误：应有 %d 个整数\n", n);
            slist_destroy(&list);
            return 0;
        }
        if (!slist_tail_insert(&list, value)) {
            fprintf(stderr, "内存不足\n");
            slist_destroy(&list);
            return 0;
        }
    }
    printf("case n=%d\n", n);
    list_rearrange(list.head, stdout);
    slist_destroy(&list);
    return 1;
}

int main(void) {
    for (;;) {
        int n = 0;
        prompt("请输入长度 n（Ctrl+D 结束）：");
        int got = scanf("%d", &n);
        if (got == EOF) {
            break;
        }
        if (got != 1 || n < 0 || n > 100000) {
            fprintf(stderr, "输入格式错误：n 应为 0 到 100000 的整数\n");
            return 1;
        }
        if (!run_case(n)) {
            return 1;
        }
    }
    return 0;
}
