/*
 * IEEE 754 的入口。编码、解码和加减的推导在 src/ 里打印。
 */
#include "prompt.h"
#include "show.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int read_line(char *buf, int n) {
    prompt("请输入命令（enc、dec、add、sub、fields、fit、cast、sum）：");
    if (fgets(buf, n, stdin) == NULL) {
        return 0;
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        fprintf(stderr, "这一行太长\n");
        return -1;
    }
    return 1;
}

static int split_line(char *line, char **argv, int cap) {
    int n = 0;
    char *p = line;
    while (*p != '\0' && n < cap) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        argv[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    return n;
}

static int parse_u(const char *text, unsigned *out) {
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value > 200) {
        return 0;
    }
    *out = (unsigned)value;
    return 1;
}

static int parse_i(const char *text, int *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int run_line(char **av, int n) {
    if (n == 0) {
        return 0;
    }
    if (strcmp(av[0], "enc") == 0) {
        return n == 2 ? show_enc(stdout, av[1]) : fail("enc 需要一个十进制数，例如 -8.25");
    }
    if (strcmp(av[0], "dec") == 0) {
        return n == 2 ? show_dec(stdout, av[1]) : fail("dec 需要一个 32 位机器数，例如 C6400000H");
    }
    if (strcmp(av[0], "add") == 0) {
        return n == 3 ? show_add(stdout, av[1], av[2], 0) : fail("add 需要两个数，例如 1.5 1.5");
    }
    if (strcmp(av[0], "sub") == 0) {
        return n == 3 ? show_add(stdout, av[1], av[2], 1) : fail("sub 需要两个数，例如 1 0.25");
    }
    if (strcmp(av[0], "fields") == 0) {
        return n == 1 ? show_fields(stdout) : fail("fields 不带参数");
    }
    if (strcmp(av[0], "fit") == 0) {
        int ka;
        int kb;
        if (n != 3 || !parse_i(av[1], &ka) || !parse_i(av[2], &kb)) {
            return fail("fit 需要两个非负整数，例如 20 40");
        }
        return show_fit(stdout, ka, kb);
    }
    if (strcmp(av[0], "cast") == 0) {
        return n == 2 ? show_cast(stdout, av[1]) : fail("cast 需要一个数，例如 16777217");
    }
    if (strcmp(av[0], "sum") == 0) {
        unsigned k;
        if (n != 2 || !parse_u(av[1], &k)) {
            return fail("sum 需要 n，例如 23");
        }
        return show_sum(stdout, k);
    }
    return fail("命令是 enc、dec、add、sub、fields、fit、cast、sum");
}

int main(void) {
    char line[256];
    int rc = 0;
    for (;;) {
        char *av[8];
        int n;
        int got = read_line(line, (int)sizeof line);
        if (got == 0) {
            break;
        }
        if (got < 0) {
            return 1;
        }
        n = split_line(line, av, 8);
        if (run_line(av, n) != 0) {
            rc = 1;
        }
    }
    return rc;
}
