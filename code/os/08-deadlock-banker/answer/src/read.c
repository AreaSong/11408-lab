#include "read.h"

#include "fmt.h"
#include "prompt.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_line(char *buf, int cap, const char *tip) {
    prompt("%s", tip);
    if (fgets(buf, cap, stdin) == NULL) {
        return 1;
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != EOF && ch != '\n') {
        }
        return -1;
    }
    while (buf[0] != '\0') {
        size_t n = strlen(buf);
        if (n == 0 || (buf[n - 1] != '\n' && buf[n - 1] != '\r')) {
            break;
        }
        buf[n - 1] = '\0';
    }
    return 0;
}

int parse_ints(const char *text, int *out, int n) {
    const char *p = text;
    int i;
    for (i = 0; i < n; i++) {
        char *end = NULL;
        long value;
        errno = 0;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            return -1;
        }
        value = strtol(p, &end, 10);
        if (end == p || errno == ERANGE || value < 0 || value > BANK_MAXV) {
            return -1;
        }
        if (*end != '\0' && *end != ' ' && *end != '\t') {
            return -1;
        }
        out[i] = (int)value;
        p = end;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '\0' ? 0 : -1;
}

int split_tok(const char *text, char tok[][TOK_LEN], int max, int *ntok) {
    const char *p = text;
    *ntok = 0;
    while (*p != '\0') {
        int k = 0;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (*ntok >= max) {
            return -1;
        }
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            if (k >= TOK_LEN - 1) {
                return -1;
            }
            tok[*ntok][k++] = *p++;
        }
        tok[*ntok][k] = '\0';
        (*ntok)++;
    }
    return 0;
}

int read_rows(int n, int m, int base, int rows[][BANK_M], const char *what) {
    int i;
    for (i = 0; i < n; i++) {
        char line[256];
        char tip[128];
        char name[16];
        int rc;
        fmt_name(name, (int)sizeof name, i, base);
        snprintf(tip, sizeof tip, "请输入 %s 的%s：", name, what);
        rc = read_line(line, (int)sizeof line, tip);
        if (rc == 1) {
            printf("missing-line\n");
            return -1;
        }
        if (rc != 0) {
            printf("bad-line\n");
            return -1;
        }
        if (parse_ints(line, rows[i], m) != 0) {
            printf("bad-number\n");
            return -1;
        }
    }
    return 0;
}
