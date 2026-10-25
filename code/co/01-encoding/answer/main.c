/*
 * 编码、进制、大小端和对齐的入口。算式在 src/ 里打印，这里只拆命令。
 */
#include "bits.h"
#include "code.h"
#include "mem.h"
#include "prompt.h"
#include "radix.h"

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

static bool parse_int(const char *text, int *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    *out = (int)value;
    return true;
}

static int parse_signedness(const char *text, bool *signed_value) {
    if (strcmp(text, "signed") == 0) {
        *signed_value = true;
        return 1;
    }
    if (strcmp(text, "unsigned") == 0) {
        *signed_value = false;
        return 1;
    }
    return 0;
}

static int run_base(char **av, int n) {
    int from = 0;
    int to = 0;
    int digits = 0;
    if (n != 5 || !parse_int(av[1], &from) || !parse_int(av[2], &to) || !parse_int(av[3], &digits)) {
        return fail("base 需要：源进制 目标进制 小数位数 数值");
    }
    return radix_print(stdout, from, to, digits, av[4]);
}

static int run_code(char **av, int n) {
    int width = 0;
    if (n != 4 || !parse_int(av[2], &width)) {
        return fail("code 需要：int 或 frac、位宽、真值");
    }
    if (strcmp(av[1], "int") == 0) {
        return code_print_int(stdout, width, av[3]);
    }
    if (strcmp(av[1], "frac") == 0) {
        return code_print_frac(stdout, width, av[3]);
    }
    return fail("code 的种类用 int 或 frac");
}

static int run_decode(char **av, int n) {
    int width = 0;
    CodeKind kind;
    if (n != 5 || !parse_int(av[2], &width) || !code_kind_parse(av[3], &kind)) {
        return fail("decode 需要：int 或 frac、位宽、orig/ones/comp/bias/unsigned、机器数");
    }
    if (strcmp(av[1], "int") != 0 && strcmp(av[1], "frac") != 0) {
        return fail("decode 的种类用 int 或 frac");
    }
    return code_print_decode(stdout, strcmp(av[1], "frac") == 0, width, kind, av[4]);
}

static int run_range(char **av, int n) {
    int width = 0;
    if (n != 3 || !parse_int(av[2], &width)) {
        return fail("range 需要：int 或 frac、位宽");
    }
    if (strcmp(av[1], "int") == 0) {
        return code_print_range(stdout, false, width);
    }
    if (strcmp(av[1], "frac") == 0) {
        return code_print_range(stdout, true, width);
    }
    return fail("range 的种类用 int 或 frac");
}

static int run_retype(char **av, int n) {
    bool src_signed = false;
    bool dst_signed = false;
    int src_w = 0;
    int dst_w = 0;
    if (n != 6 || !parse_signedness(av[1], &src_signed) || !parse_signedness(av[2], &dst_signed) ||
        !parse_int(av[3], &src_w) || !parse_int(av[4], &dst_w)) {
        return fail("retype 需要：signed/unsigned、signed/unsigned、源位宽、目标位宽、真值");
    }
    return code_print_retype(stdout, src_signed, dst_signed, src_w, dst_w, av[5]);
}

static int run_extend(char **av, int n) {
    int from_w = 0;
    int to_w = 0;
    if (n != 5 || strcmp(av[1], "orig") != 0 || !parse_int(av[2], &from_w) || !parse_int(av[3], &to_w)) {
        return fail("extend 需要：orig、源位宽、目标位宽、原码机器数");
    }
    return code_print_orig_extend(stdout, from_w, to_w, av[4]);
}

static int run_sum(char **av, int n) {
    int width = 0;
    if (n != 4 || !parse_int(av[1], &width)) {
        return fail("sum 需要：位宽、机器数、机器数");
    }
    return code_print_sum(stdout, width, av[2], av[3]);
}

static int run_cmp(char **av, int n) {
    int width = 0;
    if (n != 5 || !parse_int(av[1], &width)) {
        return fail("cmp 需要：位宽、x、y、z 三个机器数");
    }
    return code_print_cmp(stdout, width, av[2], av[3], av[4]);
}

static int run_endian(char **av, int n) {
    unsigned long long addr = 0;
    unsigned long long query = 0;
    int nbytes = 0;
    char err[120];
    if (n != 5 && n != 6) {
        return fail("endian 需要：le/be/both、首地址、字节数、机器数，可选要查的地址");
    }
    if (!bits_parse_hex(av[2], &addr, err, sizeof err) || !parse_int(av[3], &nbytes)) {
        return fail("端序的地址按十六进制，字节数用十进制");
    }
    if (n == 6 && !bits_parse_hex(av[5], &query, err, sizeof err)) {
        return fail(err);
    }
    return mem_print_endian(stdout, av[1], addr, nbytes, av[4], n == 6, query);
}

static int read_members(Member *ms, int *count) {
    char line[160];
    *count = 0;
    for (;;) {
        bool is_end = false;
        char err[160];
        int got = read_line(line, (int)sizeof line, "请输入下一个成员，或 end：");
        if (got == 0) {
            return fail("成员列表要以 end 结束");
        }
        if (got < 0) {
            return 1;
        }
        if (line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        if (*count >= MEM_MAX_MEMBER) {
            return fail("成员最多 16 个");
        }
        if (!mem_parse_member(line, &ms[*count], &is_end, err, sizeof err)) {
            if (strcmp(err, "空行") == 0) {
                continue;
            }
            return fail(err);
        }
        if (is_end) {
            return 0;
        }
        (*count)++;
    }
}

static int run_align(char **av, int n) {
    Member ms[MEM_MAX_MEMBER];
    unsigned long long base = 0;
    int index = 0;
    int find_byte = 0;
    int count = 0;
    int i = 3;
    bool index_set = false;
    bool find_set = false;
    bool little = false;
    char err[120];
    if (n < 3 || !bits_parse_hex(av[1], &base, err, sizeof err)) {
        return fail("align 需要：首地址、le 或 be，可选 index 和 find");
    }
    if (strcmp(av[2], "le") == 0) {
        little = true;
    } else if (strcmp(av[2], "be") == 0) {
        little = false;
    } else {
        return fail("端序用 le 或 be");
    }
    while (i < n) {
        unsigned long long byte = 0;
        if (strcmp(av[i], "index") == 0 && i + 1 < n && parse_int(av[i + 1], &index)) {
            index_set = true;
            i += 2;
        } else if (strcmp(av[i], "find") == 0 && i + 1 < n && bits_parse_hex(av[i + 1], &byte, err, sizeof err) &&
                   byte <= 255) {
            find_set = true;
            find_byte = (int)byte;
            i += 2;
        } else {
            return fail("align 的可选项是 index <下标> 和 find <两位十六进制>");
        }
    }
    if (read_members(ms, &count) != 0) {
        return 1;
    }
    return mem_print_align(stdout, base, little, index, index_set, find_byte, find_set, ms, count);
}

int main(void) {
    char line[256];
    const char *tip = "请输入命令（base / code / decode / range / retype / extend / sum / cmp / endian / align，"
                      "Ctrl+D 结束）：";
    for (;;) {
        char *av[12];
        int n;
        int got = read_line(line, (int)sizeof line, tip);
        int rc;
        if (got == 0) {
            break;
        }
        if (got < 0) {
            return 1;
        }
        n = split_line(line, av, 12);
        if (n == 0) {
            continue;
        }
        if (strcmp(av[0], "base") == 0) {
            rc = run_base(av, n);
        } else if (strcmp(av[0], "code") == 0) {
            rc = run_code(av, n);
        } else if (strcmp(av[0], "decode") == 0) {
            rc = run_decode(av, n);
        } else if (strcmp(av[0], "range") == 0) {
            rc = run_range(av, n);
        } else if (strcmp(av[0], "retype") == 0) {
            rc = run_retype(av, n);
        } else if (strcmp(av[0], "extend") == 0) {
            rc = run_extend(av, n);
        } else if (strcmp(av[0], "sum") == 0) {
            rc = run_sum(av, n);
        } else if (strcmp(av[0], "cmp") == 0) {
            rc = run_cmp(av, n);
        } else if (strcmp(av[0], "endian") == 0) {
            rc = run_endian(av, n);
        } else if (strcmp(av[0], "align") == 0) {
            rc = run_align(av, n);
        } else {
            rc = fail("未知命令");
        }
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}
