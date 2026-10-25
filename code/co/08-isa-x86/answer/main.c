#include "addr.h"
#include "form.h"
#include "jcc.h"
#include "num.h"
#include "opcode.h"
#include "prompt.h"
#include "x86.h"

#include <stdio.h>
#include <string.h>

static void trim(char *text) {
    char *start = text;
    char *end;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
}

static int arg_i64(char **tok, int index, int64_t *out) {
    return num_parse(tok[index], out);
}

static int arg_u64(char **tok, int index, uint64_t *out) {
    int64_t value = 0;
    if (num_parse(tok[index], &value) != 0 || value < 0) {
        return -1;
    }
    *out = (uint64_t)value;
    return 0;
}

static int arg_int(char **tok, int index, int *out) {
    int64_t value = 0;
    if (num_parse(tok[index], &value) != 0 || value > 1000000 || value < -1000000) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static int read_counts(char **tok, int n, int from, long long *counts, int *count) {
    int i;
    *count = n - from;
    if (*count < 1 || *count > OPCODE_LEVELS) {
        return -1;
    }
    for (i = 0; i < *count; i++) {
        int64_t value = 0;
        if (arg_i64(tok, from + i, &value) != 0) {
            return -1;
        }
        counts[i] = value;
    }
    return 0;
}

static int cmd_kind(char **tok, int n) {
    int naddr = 0;
    if (n != 2 || arg_int(tok, 1, &naddr) != 0) {
        return -1;
    }
    return show_kind(stdout, naddr);
}

static int cmd_ext(char **tok, int n) {
    int word = 0;
    int addr = 0;
    int top = 0;
    int count = 0;
    long long counts[OPCODE_LEVELS];
    if (n < 5 || arg_int(tok, 1, &word) != 0 || arg_int(tok, 2, &addr) != 0 || arg_int(tok, 3, &top) != 0) {
        return -1;
    }
    if (read_counts(tok, n, 4, counts, &count) != 0) {
        return -1;
    }
    return show_ext(stdout, word, addr, top, counts, count);
}

static int cmd_minlen(char **tok, int n) {
    int addr = 0;
    int align = 0;
    int top = 0;
    int count = 0;
    long long counts[OPCODE_LEVELS];
    if (n < 5 || arg_int(tok, 1, &addr) != 0 || arg_int(tok, 2, &align) != 0 || arg_int(tok, 3, &top) != 0) {
        return -1;
    }
    if (read_counts(tok, n, 4, counts, &count) != 0) {
        return -1;
    }
    return show_minlen(stdout, addr, align, top, counts, count);
}

static int cmd_direct(char **tok, int n) {
    int word = 0;
    uint64_t ninstr = 0;
    uint64_t nmodes = 0;
    if (n != 4 || arg_int(tok, 1, &word) != 0 || arg_u64(tok, 2, &ninstr) != 0 || arg_u64(tok, 3, &nmodes) != 0) {
        return -1;
    }
    return show_direct(stdout, word, ninstr, nmodes);
}

static int cmd_disp(char **tok, int n) {
    int word = 0;
    int op = 0;
    int nreg = 0;
    int fields = 0;
    if (n != 5 || arg_int(tok, 1, &word) != 0 || arg_int(tok, 2, &op) != 0 || arg_int(tok, 3, &nreg) != 0 ||
        arg_int(tok, 4, &fields) != 0) {
        return -1;
    }
    return show_disp(stdout, word, op, nreg, fields);
}

static int cmd_rel(char **tok, int n) {
    uint64_t inst = 0;
    uint64_t len = 0;
    uint64_t raw = 0;
    int width = 0;
    int scale = 0;
    if (n != 6 || arg_u64(tok, 1, &inst) != 0 || arg_u64(tok, 2, &len) != 0 || arg_u64(tok, 3, &raw) != 0 ||
        arg_int(tok, 4, &width) != 0 || arg_int(tok, 5, &scale) != 0) {
        return -1;
    }
    return show_rel(stdout, inst, len, raw, width, scale);
}

static int cmd_byte(const char *text, int *out) {
    char buf[8];
    int64_t value = 0;
    size_t n = strlen(text);
    size_t i;
    int hex = n > 0 && n <= 2;
    for (i = 0; hex && i < n; i++) {
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f') ||
              (text[i] >= 'A' && text[i] <= 'F'))) {
            hex = 0;
        }
    }
    if (hex) {
        snprintf(buf, sizeof buf, "%sH", text);
        if (num_parse(buf, &value) == 0 && value >= 0 && value <= 255) {
            *out = (int)value;
            return 0;
        }
    }
    if (num_parse(text, &value) == 0 && value >= 0 && value <= 255) {
        *out = (int)value;
        return 0;
    }
    return -1;
}

static int cmd_reloff(char **tok, int n) {
    uint64_t inst = 0;
    uint64_t len = 0;
    uint64_t target = 0;
    int bits = 0;
    int obs[4];
    int i;
    if (n < 5 || arg_u64(tok, 1, &inst) != 0 || arg_u64(tok, 2, &len) != 0 || arg_u64(tok, 3, &target) != 0 ||
        arg_int(tok, 4, &bits) != 0 || n > 9) {
        return -1;
    }
    for (i = 5; i < n; i++) {
        if (cmd_byte(tok[i], &obs[i - 5]) != 0) {
            return -1;
        }
    }
    return show_reloff(stdout, inst, len, target, bits, obs, n - 5);
}

static int cmd_span(char **tok, int n) {
    uint64_t first = 0;
    uint64_t last = 0;
    if (n != 3 || arg_u64(tok, 1, &first) != 0 || arg_u64(tok, 2, &last) != 0) {
        return -1;
    }
    return show_span(stdout, first, last);
}

static int cmd_iset(char **tok, int n) {
    int lens[8];
    int i;
    if (n < 2 || n > 9) {
        return -1;
    }
    for (i = 1; i < n; i++) {
        if (arg_int(tok, i, &lens[i - 1]) != 0) {
            return -1;
        }
    }
    return show_iset(stdout, lens, n - 1);
}

static int cmd_arr(char **tok, int n) {
    uint64_t base = 0;
    uint64_t ea = 0;
    uint64_t scale = 0;
    if (n != 4 || arg_u64(tok, 1, &base) != 0 || arg_u64(tok, 2, &ea) != 0 || arg_u64(tok, 3, &scale) != 0) {
        return -1;
    }
    return show_arr(stdout, base, ea, scale);
}

static int cmd_operand(char **tok, int n) {
    int64_t args[4];
    int i;
    if (n < 3 || n > 6) {
        return -1;
    }
    for (i = 2; i < n; i++) {
        if (arg_i64(tok, i, &args[i - 2]) != 0) {
            return -1;
        }
    }
    return show_operand(stdout, tok[1], args, n - 2);
}

static int cmd_jump(char **tok, int n) {
    if (n != 2) {
        return -1;
    }
    return show_jump(stdout, tok[1]);
}

static int cmd_x86(void) {
    char store[48][96];
    const char *lines[48];
    char buf[96];
    X86End end;
    int n = 0;
    prompt("请逐行输入汇编，标签单独成行，最后一行只写 . ：\n");
    while (fgets(buf, sizeof buf, stdin) != NULL) {
        trim(buf);
        if (buf[0] == '\0') {
            continue;
        }
        if (strcmp(buf, ".") == 0) {
            break;
        }
        if (n >= 48) {
            fprintf(stderr, "指令太多\n");
            return -1;
        }
        snprintf(store[n], sizeof store[n], "%s", buf);
        lines[n] = store[n];
        n++;
    }
    return x86_run(stdout, lines, n, &end);
}

static int dispatch(char *line) {
    char *tok[12];
    int n = num_split(line, tok, 12);
    if (n < 1) {
        return 0;
    }
    if (strcmp(tok[0], "kind") == 0) {
        return cmd_kind(tok, n);
    }
    if (strcmp(tok[0], "ext") == 0) {
        return cmd_ext(tok, n);
    }
    if (strcmp(tok[0], "minlen") == 0) {
        return cmd_minlen(tok, n);
    }
    if (strcmp(tok[0], "direct") == 0) {
        return cmd_direct(tok, n);
    }
    if (strcmp(tok[0], "disp") == 0) {
        return cmd_disp(tok, n);
    }
    if (strcmp(tok[0], "rel") == 0) {
        return cmd_rel(tok, n);
    }
    if (strcmp(tok[0], "reloff") == 0) {
        return cmd_reloff(tok, n);
    }
    if (strcmp(tok[0], "span") == 0) {
        return cmd_span(tok, n);
    }
    if (strcmp(tok[0], "iset") == 0) {
        return cmd_iset(tok, n);
    }
    if (strcmp(tok[0], "arr") == 0) {
        return cmd_arr(tok, n);
    }
    if (strcmp(tok[0], "operand") == 0) {
        return cmd_operand(tok, n);
    }
    if (strcmp(tok[0], "jump") == 0) {
        return cmd_jump(tok, n);
    }
    if (strcmp(tok[0], "x86") == 0 && n == 1) {
        return cmd_x86();
    }
    fprintf(stderr, "命令不正确\n");
    return -1;
}

int main(void) {
    char line[256];
    while (1) {
        prompt("请输入命令（kind、ext、minlen、direct、disp、rel、reloff、span、iset、arr、operand、jump、x86）：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            return 0;
        }
        trim(line);
        if (line[0] == '\0') {
            continue;
        }
        if (dispatch(line) != 0) {
            fprintf(stderr, "这条命令没有算成\n");
            return 1;
        }
    }
}
