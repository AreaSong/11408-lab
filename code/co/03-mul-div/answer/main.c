/*
 * 定点乘除的入口。原码一位乘、Booth、加减交替的推导在 src/ 里打印。
 */
#include "booth.h"
#include "divide.h"
#include "exam.h"
#include "orig.h"
#include "prompt.h"
#include "word.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int read_line(char *buf, int n) {
    prompt("请输入命令（orig、booth、div、prod、impl、cdiv）：");
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

static int parse_int(const char *text, int *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int load_frac(const char *text, Frac *out) {
    char err[120];
    if (!frac_parse(text, out, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 0;
    }
    return 1;
}

static int load_word(const char *text, int width, Word *out) {
    char err[120];
    if (!word_parse(text, width, out, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 0;
    }
    return 1;
}

static int run_orig(char **av, int n) {
    Frac x;
    Frac y;
    Orig o;
    char err[120];
    if (n != 3) {
        return fail("orig 需要两个定点小数，例如 -0.1101 +0.1011");
    }
    if (!load_frac(av[1], &x) || !load_frac(av[2], &y)) {
        return 1;
    }
    if (!orig_eval(x, y, &o, err, sizeof err)) {
        return fail(err);
    }
    return orig_print(stdout, &o);
}

static int run_booth(char **av, int n) {
    Frac x;
    Frac y;
    Booth o;
    char err[120];
    if (n != 3) {
        return fail("booth 需要两个定点小数，例如 -0.1101 +0.1011");
    }
    if (!load_frac(av[1], &x) || !load_frac(av[2], &y)) {
        return 1;
    }
    if (!booth_eval(x, y, &o, err, sizeof err)) {
        return fail(err);
    }
    return booth_print(stdout, &o);
}

static int run_div(char **av, int n) {
    Frac x;
    Frac y;
    Divide o;
    char err[120];
    if (n != 3) {
        return fail("div 需要两个定点小数，例如 +0.1001 +0.1101");
    }
    if (!load_frac(av[1], &x) || !load_frac(av[2], &y)) {
        return 1;
    }
    if (!divide_eval(x, y, &o, err, sizeof err)) {
        return fail(err);
    }
    return divide_print(stdout, &o);
}

static int run_prod(char **av, int n) {
    int width = 0;
    Word x;
    Word y;
    if (n != 4 || !parse_int(av[1], &width)) {
        return fail("prod 需要：位宽、两个机器数");
    }
    if (!load_word(av[2], width, &x) || !load_word(av[3], width, &y)) {
        return 1;
    }
    return prod_print(stdout, x, y);
}

static int run_impl(char **av, int n) {
    if (n != 2) {
        return fail("impl 需要 speed 或 choice");
    }
    return impl_print(stdout, av[1]);
}

static int run_cdiv(char **av, int n) {
    Word dividend;
    Word divisor;
    if (n != 3) {
        return fail("cdiv 需要两个 32 位机器数：被除数 d[i]、除数 x");
    }
    if (!load_word(av[1], 32, &dividend) || !load_word(av[2], 32, &divisor)) {
        return 1;
    }
    return cdiv_print(stdout, dividend, divisor);
}

static int dispatch(char **av, int n) {
    if (strcmp(av[0], "orig") == 0) {
        return run_orig(av, n);
    }
    if (strcmp(av[0], "booth") == 0) {
        return run_booth(av, n);
    }
    if (strcmp(av[0], "div") == 0) {
        return run_div(av, n);
    }
    if (strcmp(av[0], "prod") == 0) {
        return run_prod(av, n);
    }
    if (strcmp(av[0], "impl") == 0) {
        return run_impl(av, n);
    }
    if (strcmp(av[0], "cdiv") == 0) {
        return run_cdiv(av, n);
    }
    return fail("命令用 orig、booth、div、prod、impl、cdiv");
}

int main(void) {
    char line[256];
    char *av[8];
    int got;
    while ((got = read_line(line, (int)sizeof line)) == 1) {
        int n = split_line(line, av, 8);
        int rc;
        if (n == 0) {
            continue;
        }
        rc = dispatch(av, n);
        if (rc != 0) {
            return rc;
        }
    }
    return got < 0 ? 1 : 0;
}
