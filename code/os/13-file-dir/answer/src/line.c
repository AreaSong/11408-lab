#include "line.h"

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

int need(int got, int want) {
    if (got != want) {
        return fail("bad-command");
    }
    return 0;
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

int read_i(const char *text, int *out) {
    char *end = 0;
    long value;

    if (text == 0 || !isdigit((unsigned char)text[0])) {
        return -1;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 1000000) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

int read_mode(const char *text, int *out) {
    int mode = 0;
    int i;

    if (text == 0 || strlen(text) != 3) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (text[i] < '0' || text[i] > '7') {
            return -1;
        }
        mode = (mode << 3) | (text[i] - '0');
    }
    *out = mode;
    return 0;
}
