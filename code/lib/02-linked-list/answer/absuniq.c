/*
 * 2015-41 的入口。每组是上界 n、结点个数 m，再跟 m 个整数。
 */
#include <stdio.h>

#include "absuniq.h"
#include "prompt.h"
#include "slist.h"

static int run_case(int n, int m) {
    SList list;
    int removed;
    if (!slist_init(&list)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    if (m > 0) {
        prompt("请输入 %d 个整数：", m);
    }
    for (int i = 0; i < m; i++) {
        int value = 0;
        if (scanf("%d", &value) != 1) {
            fprintf(stderr, "输入格式错误：应有 %d 个整数\n", m);
            slist_destroy(&list);
            return 0;
        }
        if (!slist_tail_insert(&list, value)) {
            fprintf(stderr, "内存不足\n");
            slist_destroy(&list);
            return 0;
        }
    }
    removed = list_abs_unique(list.head, n);
    if (removed < 0) {
        fprintf(stderr, "输入格式错误：绝对值应在 0 到 %d 之间\n", n);
        slist_destroy(&list);
        return 0;
    }
    printf("case n=%d m=%d removed=%d ", n, m, removed);
    slist_print(&list, stdout);
    fputc('\n', stdout);
    slist_destroy(&list);
    return 1;
}

int main(void) {
    for (;;) {
        int n = 0;
        int m = 0;
        int got;
        prompt("请输入上界 n 和结点个数 m（Ctrl+D 结束）：");
        got = scanf("%d %d", &n, &m);
        if (got == EOF) {
            break;
        }
        if (got != 2 || n < 1 || n > 100000 || m < 0 || m > 100000) {
            fprintf(stderr, "输入格式错误：n 为 1 到 100000，m 为 0 到 100000\n");
            return 1;
        }
        if (!run_case(n, m)) {
            return 1;
        }
    }
    return 0;
}
