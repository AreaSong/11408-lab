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

static int parse_i(const char *text, long long *out) {
    char *end = 0;
    long long value;
    int neg = 0;

    if (text == 0 || text[0] == '\0') {
        return -1;
    }
    if (text[0] == '-') {
        neg = 1;
        text++;
        if (text[0] == '\0') {
            return -1;
        }
    }
    if (!isdigit((unsigned char)text[0])) {
        return -1;
    }
    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }
    *out = neg ? -value : value;
    return 0;
}

int take_i(const char *text, int lo, int hi, int *out) {
    long long value;

    if (parse_i(text, &value) != 0 || value < lo || value > hi) {
        return fail("bad-number");
    }
    *out = (int)value;
    return 0;
}

int take_u64(const char *text, unsigned long long lo, unsigned long long hi,
             unsigned long long *out) {
    char *end = 0;
    unsigned long long value;

    if (text == 0 || !isdigit((unsigned char)text[0])) {
        return fail("bad-number");
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < lo || value > hi) {
        return fail("bad-number");
    }
    *out = value;
    return 0;
}

static int name_char(unsigned char ch) {
    return isalnum(ch) || ch == '.' || ch == '_' || ch == '-';
}

int token_name(const char *text, int lo, int hi) {
    int n = 0;

    if (text == 0) {
        return -1;
    }
    while (text[n] != '\0') {
        if (!name_char((unsigned char)text[n]) || n >= hi) {
            return -1;
        }
        n++;
    }
    return n < lo ? -1 : 0;
}

int token_path(const char *text) {
    int n = 0;

    if (text == 0 || text[0] == '\0') {
        return -1;
    }
    while (text[n] != '\0') {
        unsigned char ch = (unsigned char)text[n];
        if (!name_char(ch) && ch != '/') {
            return -1;
        }
        if (++n > 200) {
            return -1;
        }
    }
    return 0;
}
