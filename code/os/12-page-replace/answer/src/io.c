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

int read_range(const char *text, int lo, int hi, int *out) {
    char *end = NULL;
    long value;

    if (text == NULL || text[0] == '\0' || !isdigit((unsigned char)text[0])) {
        return -1;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < lo || value > hi) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

int parse_ref(const char *text, Access *out) {
    char buf[TOK_W];
    size_t n;
    int page;
    int write = 0;

    if (text == NULL) {
        return -1;
    }
    n = strlen(text);
    if (n == 0 || n >= TOK_W) {
        return -1;
    }
    if (text[n - 1] == 'r' || text[n - 1] == 'w') {
        write = text[n - 1] == 'w';
        n--;
    }
    if (n == 0) {
        return -1;
    }
    memcpy(buf, text, n);
    buf[n] = '\0';
    if (read_range(buf, 0, PAGE_MAX, &page) != 0) {
        return -1;
    }
    out->page = page;
    out->write = write;
    return 0;
}

int parse_cell(const char *text, int *page, int *aref, int *dirty) {
    char *end = NULL;
    long p;
    long a;
    long m;

    if (text == NULL || !isdigit((unsigned char)text[0])) {
        return -1;
    }
    errno = 0;
    p = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != ':') {
        return -1;
    }
    a = strtol(end + 1, &end, 10);
    if (end == NULL || *end != '.') {
        return -1;
    }
    m = strtol(end + 1, &end, 10);
    if (*end != '\0' || p < 0 || p > PAGE_MAX || (a != 0 && a != 1) || (m != 0 && m != 1)) {
        return -1;
    }
    *page = (int)p;
    *aref = (int)a;
    *dirty = (int)m;
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
    if (fgets(buf, cap, stdin) == NULL) {
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
