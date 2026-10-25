#include "run_ipv6.h"

#include "ipv6.h"
#include "line.h"

#include <stdio.h>
#include <string.h>

static void print_groups(const uint16_t group[8]) {
    int i;

    printf("8 段：");
    for (i = 0; i < 8; i++) {
        printf("%s%x", i ? " " : "", group[i]);
    }
    printf("\n");
}

static void print_span(const ZeroSpan *span) {
    if (span->len == 8) {
        printf("8 段全是 0，写成 ::。\n");
        return;
    }
    if (span->len < 2) {
        printf("没有长度达到 2 段的连续 0，所以不用 ::。\n");
        return;
    }
    if (span->ties > 1) {
        printf("有 %d 处连续 0 的长度都是 %d 段，压最左边、从第 %d 段开始的那一处。\n", span->ties, span->len,
               span->start + 1);
        return;
    }
    printf("最长的连续 0 是 %d 段，从第 %d 段开始，压成 ::。:: 只能出现一次。\n", span->len, span->start + 1);
}

static int count_groups(const char *text) {
    int n = 0;
    int in = 0;
    const char *s = text;

    while (*s != '\0') {
        if (*s == ':') {
            in = 0;
        } else if (!in) {
            in = 1;
            n++;
        }
        s++;
    }
    return n;
}

static int cmd_compress(const uint16_t group[8]) {
    ZeroSpan span;
    char out[64];

    if (!ipv6_compress(group, out, (int)sizeof out)) {
        return reject("压缩结果放不下");
    }
    ipv6_zero_span(group, &span);
    printf("【压缩】\n");
    print_groups(group);
    printf("每段去掉前导零，字母用小写。\n");
    print_span(&span);
    printf("结果：%s\n", out);
    return 0;
}

static int cmd_expand(const char *text, const uint16_t group[8]) {
    char out[64];
    int filled = 8 - count_groups(text);

    if (!ipv6_expand(group, out, (int)sizeof out)) {
        return reject("展开结果放不下");
    }
    printf("【展开】\n");
    if (strstr(text, "::") != NULL) {
        printf(":: 补上了 %d 段 0。每段再写成 4 位十六进制。\n", filled);
    } else {
        printf("已经有 8 段，只把每段补成 4 位十六进制。\n");
    }
    printf("结果：%s\n", out);
    return 0;
}

int run_ipv6(char *line) {
    char cmd[16];
    char text[64];
    uint16_t group[8];

    if (take_token(&line, cmd, (int)sizeof cmd) != 1 || take_token(&line, text, (int)sizeof text) != 1 ||
        !rest_blank(line)) {
        return reject("用法：compress <地址> 或 expand <地址>");
    }
    if (!ipv6_parse(text, group)) {
        return reject("IPv6 地址不合法");
    }
    if (strcmp(cmd, "compress") == 0) {
        return cmd_compress(group);
    }
    if (strcmp(cmd, "expand") == 0) {
        return cmd_expand(text, group);
    }
    return reject("不认识的命令");
}
