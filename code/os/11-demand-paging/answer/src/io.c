#include "io.h"

#include "prompt.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fail(const char *msg) {
    printf("%s\n", msg);
    return 1;
}

int need_args(int got, int want) {
    if (got != want) {
        return fail("bad-command");
    }
    return 0;
}

/* 十进制、0x、或末尾 H。H 是考卷上的十六进制记号。010 是十进制 10。 */
int read_ull(const char *text, unsigned long long *out) {
    char buf[TOK_W];
    char *end = 0;
    size_t n;
    int base = 10;
    unsigned long long value;

    if (text == 0 || text[0] == '\0') {
        return -1;
    }
    n = strlen(text);
    if (n >= TOK_W) {
        return -1;
    }
    memcpy(buf, text, n + 1);
    if (buf[n - 1] == 'H' || buf[n - 1] == 'h') {
        buf[--n] = '\0';
        base = 16;
    } else if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
        base = 16;
    }
    if (n == 0 || !isxdigit((unsigned char)buf[0])) {
        return -1;
    }
    errno = 0;
    value = strtoull(buf, &end, base);
    if (errno != 0 || end == buf || *end != '\0') {
        return -1;
    }
    *out = value;
    return 0;
}

int read_args(char tok[][TOK_W], int from, int n, unsigned long long *out) {
    int i;

    for (i = 0; i < n; i++) {
        if (read_ull(tok[from + i], &out[i]) != 0) {
            return fail("bad-number");
        }
    }
    return 0;
}

void print_hex(unsigned long long value) {
    printf("0x%llx", value);
}

int split_toks(const char *text, char tok[][TOK_W], int *ntok) {
    int n = 0;

    while (*text != '\0') {
        int k = 0;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
        if (*text == '\0') {
            break;
        }
        if (n >= TOK_N) {
            return -1;
        }
        while (*text != '\0' && *text != ' ' && *text != '\t') {
            if (k >= TOK_W - 1) {
                return -1;
            }
            tok[n][k++] = *text++;
        }
        tok[n][k] = '\0';
        n++;
    }
    *ntok = n;
    return n == 0 ? -1 : 0;
}

int read_cmd(char *buf, int cap) {
    size_t n;

    prompt("请输入命令，Ctrl+D 结束：");
    if (fgets(buf, cap, stdin) == 0) {
        return 1;
    }
    n = strlen(buf);
    if (n == 0 || buf[n - 1] != '\n') {
        int ch;
        if (n == (size_t)cap - 1) {
            while ((ch = getchar()) != '\n' && ch != EOF) {
            }
        }
        printf("bad-line\n");
        return -1;
    }
    buf[n - 1] = '\0';
    if (n >= 2 && buf[n - 2] == '\r') {
        buf[n - 2] = '\0';
    }
    return 0;
}
