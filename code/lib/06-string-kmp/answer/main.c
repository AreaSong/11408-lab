#include "match.h"
#include "nexttab.h"
#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_line(void) {
    int c;
    while ((c = getchar()) != EOF && c != '\n') {
    }
}

/* 1 读到；0 这一行没有下一个词；-1 文件结束；-2 超过 STR_MAX。不跨行取词。 */
static int read_token(char *buf) {
    int c = getchar();
    int n = 0;
    while (c == ' ' || c == '\t' || c == '\r') {
        c = getchar();
    }
    if (c == '\n') {
        ungetc(c, stdin);
        return 0;
    }
    if (c == EOF) {
        return -1;
    }
    while (c != EOF && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        if (n >= STR_MAX) {
            while (c != EOF && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                c = getchar();
            }
            if (c != EOF) {
                ungetc(c, stdin);
            }
            buf[0] = '\0';
            return -2;
        }
        buf[n++] = (char)c;
        c = getchar();
    }
    if (c != EOF) {
        ungetc(c, stdin);
    }
    buf[n] = '\0';
    return 1;
}

static int read_base(int *base) {
    char buf[STR_MAX + 1];
    char *end = NULL;
    long value;
    int got = read_token(buf);
    if (got != 1) {
        return got;
    }
    value = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || value < -999999 || value > 999999) {
        return 0;
    }
    *base = (int)value;
    return 1;
}

static int reject(const char *what, int code) {
    if (code == 0) {
        printf("fail empty %s\n", what);
    } else if (code == -1) {
        printf("fail long %s\n", what);
    } else {
        printf("fail char %s\n", what);
    }
    return 1;
}

/* 1 读到一个词；2 超长，已经打印 fail long；0 缺词；-1 文件结束。 */
static int take(char *buf, const char *what) {
    int got = read_token(buf);
    if (got == -2) {
        printf("fail long %s\n", what);
        return 2;
    }
    return got;
}

/* 词有效返回 1。否则 *rc 是本条命令该返回的码：超长已经处理过，返回 1 继续读下一条。 */
static int accepted(int got, int *rc) {
    if (got == 1) {
        return 1;
    }
    *rc = (got == 2) ? 1 : got;
    return 0;
}

static int cmd_table(int use_val) {
    int base = 0;
    char pat[STR_MAX + 1];
    int got;
    int code;
    Pattern compiled;
    got = read_base(&base);
    if (got == -2) {
        printf("fail long base\n");
        return 1;
    }
    if (got != 1) {
        return got;
    }
    if (!accepted(take(pat, "pat"), &got)) {
        return got;
    }
    if (base != 0 && base != 1) {
        printf("fail base %d\n", base);
        return 1;
    }
    code = kmp_check(pat);
    if (code != 1) {
        return reject("pat", code);
    }
    if (pattern_build(&compiled, base, pat) != 0) {
        return 0;
    }
    if (use_val) {
        pattern_print_nextval(&compiled, stdout);
    } else {
        pattern_print_next(&compiled, stdout);
    }
    return 1;
}

static int cmd_bf(void) {
    char text[STR_MAX + 1];
    char pat[STR_MAX + 1];
    int got;
    int code;
    MatchInfo info;
    if (!accepted(take(text, "text"), &got)) {
        return got;
    }
    if (!accepted(take(pat, "pat"), &got)) {
        return got;
    }
    code = kmp_check(text);
    if (code != 1) {
        return reject("text", code);
    }
    code = kmp_check(pat);
    if (code != 1) {
        return reject("pat", code);
    }
    if (!match_bf(text, pat, stdout, &info)) {
        return 0;
    }
    return 1;
}

static int cmd_kmp(int use_val) {
    int base = 0;
    char text[STR_MAX + 1];
    char pat[STR_MAX + 1];
    int got;
    int code;
    MatchInfo info;
    got = read_base(&base);
    if (got == -2) {
        printf("fail long base\n");
        return 1;
    }
    if (got != 1) {
        return got;
    }
    if (!accepted(take(text, "text"), &got)) {
        return got;
    }
    if (!accepted(take(pat, "pat"), &got)) {
        return got;
    }
    if (base != 0 && base != 1) {
        printf("fail base %d\n", base);
        return 1;
    }
    code = kmp_check(text);
    if (code != 1) {
        return reject("text", code);
    }
    code = kmp_check(pat);
    if (code != 1) {
        return reject("pat", code);
    }
    if (!match_kmp(text, pat, base, use_val, stdout, &info)) {
        return 0;
    }
    return 1;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "next") == 0) {
        return cmd_table(0);
    }
    if (strcmp(cmd, "nextval") == 0) {
        return cmd_table(1);
    }
    if (strcmp(cmd, "bf") == 0) {
        return cmd_bf();
    }
    if (strcmp(cmd, "kmp") == 0) {
        return cmd_kmp(0);
    }
    if (strcmp(cmd, "kmpval") == 0) {
        return cmd_kmp(1);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[16];
    for (;;) {
        int rc;
        prompt("请输入命令（next base 模式串 / nextval base 模式串 / bf 主串 模式串 / "
               "kmp base 主串 模式串 / kmpval base 主串 模式串，一行一条，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        rc = dispatch(cmd);
        if (rc == -1) {
            return 0;
        }
        if (rc == 0) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
        skip_line();
    }
}
