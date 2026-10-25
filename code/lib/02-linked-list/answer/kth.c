/*
 * 2009-42 的入口。每组是 n、k 和 n 个整数，读到输入结束。
 * 查找只读链表，打完结果后再把链表打印出来，用来核对它没有被改掉。
 */
#include <stdio.h>

#include "kth.h"
#include "prompt.h"
#include "slist.h"

static int run_case(int n, int k) {
    SList list;
    if (!slist_init(&list)) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    if (n > 0) {
        prompt("请输入链表的 %d 个整数：", n);
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
    printf("case n=%d k=%d\n", n, k);
    int value = 0;
    list_kth(list.head, k, &value, stdout);
    fputs("list=", stdout);
    slist_print(&list, stdout);
    fputc('\n', stdout);
    slist_destroy(&list);
    return 1;
}

int main(void) {
    for (;;) {
        int n = 0;
        int k = 0;
        prompt("请输入 n 和 k（Ctrl+D 结束）：");
        int got = scanf("%d %d", &n, &k);
        if (got == EOF) {
            break;
        }
        if (got != 2 || n < 0 || n > 100000 || k < 1) {
            fprintf(stderr, "输入格式错误：n 应为 0 到 100000，k 应为正整数\n");
            return 1;
        }
        if (!run_case(n, k)) {
            return 1;
        }
    }
    return 0;
}
