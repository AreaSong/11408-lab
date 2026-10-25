/*
 * 复杂度计数实验的入口。
 * 输入：每行「编号 n」，例如 "2014-1 100"
 * 输出：每行「编号 n=<n> count=<基本语句执行次数>」
 */
#include <stdio.h>

#include "prompt.h"
#include "segments.h"

int main(void) {
    char id[32];
    int n = 0;
    for (;;) {
        prompt("请输入「编号 n」，例如 2014-1 100（Ctrl+D 结束）：");
        if (scanf("%31s %d", id, &n) != 2) {
            break;
        }
        long long count = 0;
        if (!segment_count(id, n, &count)) {
            printf("%s unknown\n", id);
            continue;
        }
        printf("%s n=%d count=%lld\n", id, n, count);
    }
    return 0;
}
