/*
 * 加法器、标志位、移位和混合运算的入口。推导在 src/ 里打印，这里只拆命令。
 */
#include "alu.h"
#include "mix.h"
#include "prompt.h"
#include "shift.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int read_line(char *buf, int n, const char *tip) {
    prompt("%s", tip);
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

static int load_word(const char *text, int width, Word *out) {
    char err[120];
    if (!word_parse(text, width, out, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 0;
    }
    return 1;
}

static int run_fa(char **av, int n) {
    int a = 0;
    int b = 0;
    int cin = 0;
    if (n != 4 || !parse_int(av[1], &a) || !parse_int(av[2], &b) || !parse_int(av[3], &cin)) {
        return fail("fa 需要：a b cin，每个是 0 或 1");
    }
    return fa_print(stdout, a, b, cin);
}

static int run_alu(char **av, int n) {
    AluOp op;
    int width = 0;
    Word a;
    Word b;
    AluResult result;
    if (n != 5 || !alu_op_parse(av[1], &op) || !parse_int(av[2], &width)) {
        return fail("alu 需要：add/sub/and/or、位宽、两个数");
    }
    if (!load_word(av[3], width, &a) || !load_word(av[4], width, &b)) {
        return 1;
    }
    result = alu_exec(op, a, b);
    return alu_print(stdout, &result);
}

static int run_cla(char **av, int n) {
    Word a;
    Word b;
    if (n != 3) {
        return fail("cla 需要两个 4 位机器数");
    }
    if (!load_word(av[1], 4, &a) || !load_word(av[2], 4, &b)) {
        return 1;
    }
    return cla_print(stdout, a, b);
}

static int run_shift(char **av, int n) {
    int arith = 0;
    int right = 0;
    int width = 0;
    int count = 0;
    Word value;
    if (n != 6 || !parse_int(av[3], &width) || !parse_int(av[5], &count)) {
        return fail("shift 需要：logic 或 arith、l 或 r、位宽、数值、位数");
    }
    if (strcmp(av[1], "arith") == 0) {
        arith = 1;
    } else if (strcmp(av[1], "logic") != 0) {
        return fail("移位种类用 logic 或 arith");
    }
    if (strcmp(av[2], "r") == 0) {
        right = 1;
    } else if (strcmp(av[2], "l") != 0) {
        return fail("方向用 l 或 r");
    }
    if (count < 0 || count > width) {
        return fail("移位次数要在 0 和位宽之间");
    }
    if (!load_word(av[4], width, &value)) {
        return 1;
    }
    return shift_print(stdout, value, arith, right, count);
}

static int run_mix(char **av, int n) {
    MixOp op;
    CType left_type;
    CType right_type;
    MixResult result;
    char err[120];
    if (n != 6) {
        return fail("mix 需要：cmp/add/sub、左类型、左值、右类型、右值");
    }
    if (strcmp(av[1], "cmp") == 0) {
        op = MIX_CMP;
    } else if (strcmp(av[1], "add") == 0) {
        op = MIX_ADD;
    } else if (strcmp(av[1], "sub") == 0) {
        op = MIX_SUB;
    } else {
        return fail("mix 的运算用 cmp、add 或 sub");
    }
    if (!ctype_parse(av[2], &left_type) || !ctype_parse(av[4], &right_type)) {
        return fail("类型用 char、uchar、short、ushort、int、unsigned");
    }
    if (!mix_eval(op, left_type, av[3], right_type, av[5], &result, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    return mix_print(stdout, &result);
}

static int run_wrap(char **av, int n) {
    int is_unsigned = 0;
    long long value = 0;
    WrapInfo info;
    char err[120];
    if (n != 3 || !parse_i64(av[2], &value)) {
        return fail("wrap 需要：unsigned 或 int、n");
    }
    if (strcmp(av[1], "unsigned") == 0) {
        is_unsigned = 1;
    } else if (strcmp(av[1], "int") != 0) {
        return fail("wrap 的类型用 unsigned 或 int");
    }
    if (!wrap_eval(is_unsigned, value, &info, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    return wrap_print(stdout, &info, is_unsigned);
}

static int run_fits(char **av, int n) {
    int is_unsigned = 0;
    long long value = 0;
    FitInfo info;
    char err[120];
    if (n != 3 || !parse_i64(av[2], &value)) {
        return fail("fits 需要：int 或 unsigned、数学值");
    }
    if (strcmp(av[1], "unsigned") == 0) {
        is_unsigned = 1;
    } else if (strcmp(av[1], "int") != 0) {
        return fail("fits 的类型用 int 或 unsigned");
    }
    if (!fit_eval(is_unsigned, value, &info, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    return fit_print(stdout, &info, is_unsigned, value);
}

static int run_read(char **av, int n) {
    int width = 0;
    int as_signed = 0;
    Word value;
    if (n != 4 || !parse_int(av[2], &width)) {
        return fail("read 需要：s 或 u、位宽、机器数");
    }
    if (strcmp(av[1], "s") == 0) {
        as_signed = 1;
    } else if (strcmp(av[1], "u") != 0) {
        return fail("read 的读法用 s（补码）或 u（无符号）");
    }
    if (!load_word(av[3], width, &value)) {
        return 1;
    }
    return read_print(stdout, as_signed, value);
}

static int dispatch(char **av, int n) {
    if (strcmp(av[0], "fa") == 0) {
        return run_fa(av, n);
    }
    if (strcmp(av[0], "alu") == 0) {
        return run_alu(av, n);
    }
    if (strcmp(av[0], "cla") == 0) {
        return run_cla(av, n);
    }
    if (strcmp(av[0], "shift") == 0) {
        return run_shift(av, n);
    }
    if (strcmp(av[0], "mix") == 0) {
        return run_mix(av, n);
    }
    if (strcmp(av[0], "wrap") == 0) {
        return run_wrap(av, n);
    }
    if (strcmp(av[0], "fits") == 0) {
        return run_fits(av, n);
    }
    if (strcmp(av[0], "cond") == 0) {
        if (n != 2) {
            return fail("cond 需要 bgt 或 ule");
        }
        return cond_print(stdout, av[1]);
    }
    if (strcmp(av[0], "read") == 0) {
        return run_read(av, n);
    }
    return fail("未知命令");
}

int main(void) {
    char line[256];
    const char *tip = "请输入命令（fa / alu / cla / shift / mix / wrap / fits / cond / read，Ctrl+D 结束）：";
    for (;;) {
        char *av[8];
        int n;
        int got = read_line(line, (int)sizeof line, tip);
        if (got == 0) {
            break;
        }
        if (got < 0) {
            return 1;
        }
        n = split_line(line, av, 8);
        if (n == 0) {
            continue;
        }
        if (dispatch(av, n) != 0) {
            return 1;
        }
    }
    return 0;
}
