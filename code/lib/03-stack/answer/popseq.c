/*
 * 出栈序列。入栈顺序固定为 1..n。
 * check n 后面跟 n 个整数；enum n；prefix n first；count n。
 */
#include <stdio.h>
#include <string.h>

#include "popseq.h"
#include "prompt.h"

static int run_check(void) {
    int n = 0;
    int seq[POP_CHECK_MAX];
    if (scanf("%d", &n) != 1) {
        return 0;
    }
    if (n < 1 || n > POP_CHECK_MAX) {
        printf("fail n=%d\n", n);
        return 1;
    }
    prompt("请输入 %d 个出栈序列中的整数：", n);
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &seq[i]) != 1) {
            return 0;
        }
    }
    printf("case check n=%d\n", n);
    popseq_check(n, seq, stdout);
    return 1;
}

static int run_enum(void) {
    int n = 0;
    if (scanf("%d", &n) != 1) {
        return 0;
    }
    if (n < 1 || n > POP_ENUM_MAX) {
        printf("fail n=%d\n", n);
        return 1;
    }
    printf("case enum n=%d\n", n);
    if (popseq_enum(n, stdout) < 0) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    return 1;
}

static int run_prefix(void) {
    int n = 0;
    int first = 0;
    if (scanf("%d %d", &n, &first) != 2) {
        return 0;
    }
    if (n < 1 || n > POP_ENUM_MAX) {
        printf("fail n=%d\n", n);
        return 1;
    }
    printf("case prefix n=%d first=%d\n", n, first);
    if (popseq_prefix(n, first, stdout) < 0) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    return 1;
}

static int run_count(void) {
    int n = 0;
    long long got = 0;
    long long cat = 0;
    if (scanf("%d", &n) != 1) {
        return 0;
    }
    got = popseq_count(n);
    if (got < 0) {
        printf("fail n=%d\n", n);
        return 1;
    }
    cat = popseq_catalan(n);
    printf("case count n=%d\ncount=%lld catalan=%lld match=%d\n", n, got, cat, got == cat);
    return 1;
}

int main(void) {
    char cmd[16];
    for (;;) {
        int ok = 1;
        prompt("请输入 check / enum / prefix / count（Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (strcmp(cmd, "check") == 0) {
            ok = run_check();
        } else if (strcmp(cmd, "enum") == 0) {
            ok = run_enum();
        } else if (strcmp(cmd, "prefix") == 0) {
            ok = run_prefix();
        } else if (strcmp(cmd, "count") == 0) {
            ok = run_count();
        } else {
            printf("fail unknown %s\n", cmd);
        }
        if (!ok) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
