#include "x86_int.h"

#include "jcc.h"
#include "num.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int reg_id(const char *name) {
    static const char *regs[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    int i;
    for (i = 0; i < 8; i++) {
        if (strcmp(name, regs[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static int parse_imm(const char *text, uint32_t *out) {
    int64_t value = 0;
    if (num_parse(text, &value) != 0) {
        return -1;
    }
    if (value < 0) {
        if (value < INT32_MIN) {
            return -1;
        }
        *out = (uint32_t)(int32_t)value;
        return 0;
    }
    if ((uint64_t)value > 0xFFFFFFFFULL) {
        return -1;
    }
    *out = (uint32_t)value;
    return 0;
}

static int parse_mem(const char *tok, int *reg, int32_t *disp) {
    char body[48];
    char rname[8];
    size_t n = strlen(tok);
    size_t rn;
    int64_t value = 0;
    if (n < 3 || tok[0] != '[' || tok[n - 1] != ']' || n - 2 >= sizeof body) {
        return -1;
    }
    memcpy(body, tok + 1, n - 2);
    body[n - 2] = '\0';
    if (sscanf(body, "%7[a-z]", rname) != 1 || reg_id(rname) < 0) {
        return -1;
    }
    *reg = reg_id(rname);
    rn = strlen(rname);
    if (body[rn] == '\0') {
        *disp = 0;
        return 0;
    }
    if (num_parse(body + rn, &value) != 0 || value > INT32_MAX || value < INT32_MIN) {
        return -1;
    }
    *disp = (int32_t)value;
    return 0;
}

static int is_mem(char **tok, int n, int i) {
    return i + 2 < n && strcmp(tok[i], "dword") == 0 && strcmp(tok[i + 1], "ptr") == 0;
}

static int set_reg_or_imm(X86Insn *ins, const char *text) {
    int reg = reg_id(text);
    if (reg >= 0) {
        ins->rs = reg;
        return 0;
    }
    ins->imm_ok = 1;
    return parse_imm(text, &ins->imm);
}

static int parse_mov(X86Insn *ins, char **tok, int n) {
    ins->op = X_MOV;
    ins->rd = ins->rs = -1;
    if (n == 3 && reg_id(tok[1]) >= 0) {
        ins->rd = reg_id(tok[1]);
        return set_reg_or_imm(ins, tok[2]);
    }
    if (n == 5 && reg_id(tok[1]) >= 0 && is_mem(tok, n, 2)) {
        ins->rd = reg_id(tok[1]);
        ins->mem_src = 1;
        return parse_mem(tok[4], &ins->mem_reg, &ins->disp);
    }
    if (n == 5 && is_mem(tok, n, 1)) {
        ins->mem_dst = 1;
        if (parse_mem(tok[3], &ins->mem_reg, &ins->disp) != 0) {
            return -1;
        }
        return set_reg_or_imm(ins, tok[4]);
    }
    return -1;
}

static int parse_alu(X86Insn *ins, char **tok, int n, int op) {
    ins->op = op;
    ins->rd = ins->rs = -1;
    if (n == 3 && reg_id(tok[1]) >= 0) {
        ins->rd = reg_id(tok[1]);
        return set_reg_or_imm(ins, tok[2]);
    }
    if (op == X_CMP && n == 5 && is_mem(tok, n, 1)) {
        ins->mem_src = 1;
        if (parse_mem(tok[3], &ins->mem_reg, &ins->disp) != 0) {
            return -1;
        }
        return set_reg_or_imm(ins, tok[4]);
    }
    return -1;
}

static int copy_name(char *dst, const char *src) {
    size_t i;
    if (!isalpha((unsigned char)src[0]) && src[0] != '_') {
        return -1;
    }
    for (i = 0; src[i] != '\0'; i++) {
        if (i + 1 >= 20 || (!isalnum((unsigned char)src[i]) && src[i] != '_')) {
            return -1;
        }
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return 0;
}

static int parse_jump(X86Insn *ins, char **tok, int n) {
    int id;
    if (n != 2) {
        return -1;
    }
    if (strcmp(tok[0], "call") == 0) {
        ins->op = X_CALL;
        return copy_name(ins->tname, tok[1]);
    }
    id = jcc_find(tok[0]);
    if (id < 0) {
        return -1;
    }
    ins->op = X_JUMP;
    ins->jcc = id;
    return copy_name(ins->tname, tok[1]);
}

static int parse_insn(X86Insn *ins, char **tok, int n) {
    if (strcmp(tok[0], "mov") == 0) {
        return parse_mov(ins, tok, n);
    }
    if (strcmp(tok[0], "add") == 0) {
        return parse_alu(ins, tok, n, X_ADD);
    }
    if (strcmp(tok[0], "sub") == 0) {
        return parse_alu(ins, tok, n, X_SUB);
    }
    if (strcmp(tok[0], "cmp") == 0) {
        return parse_alu(ins, tok, n, X_CMP);
    }
    if (strcmp(tok[0], "shl") == 0 && n == 3 && reg_id(tok[1]) >= 0) {
        ins->op = X_SHL;
        ins->rd = reg_id(tok[1]);
        ins->imm_ok = 1;
        return parse_imm(tok[2], &ins->imm);
    }
    if (strcmp(tok[0], "imul") == 0 && n == 3 && reg_id(tok[1]) >= 0 && reg_id(tok[2]) >= 0) {
        ins->op = X_IMUL;
        ins->rd = reg_id(tok[1]);
        ins->rs = reg_id(tok[2]);
        return 0;
    }
    if (strcmp(tok[0], "push") == 0 && n == 2) {
        ins->op = X_PUSH;
        ins->rs = -1;
        return set_reg_or_imm(ins, tok[1]);
    }
    if (strcmp(tok[0], "pop") == 0 && n == 2 && reg_id(tok[1]) >= 0) {
        ins->op = X_POP;
        ins->rd = reg_id(tok[1]);
        return 0;
    }
    if (strcmp(tok[0], "ret") == 0 && n == 1) {
        ins->op = X_RET;
        return 0;
    }
    return parse_jump(ins, tok, n);
}

static int take_label(const char *tok, char *pending) {
    size_t n = strlen(tok);
    char name[20];
    if (n < 2 || tok[n - 1] != ':') {
        return 0;
    }
    if (n - 1 >= sizeof name) {
        return -1;
    }
    memcpy(name, tok, n - 1);
    name[n - 1] = '\0';
    if (pending[0] != '\0' || copy_name(pending, name) != 0) {
        return -1;
    }
    return 1;
}

static int find_label(const X86Insn *prog, int n, const char *name) {
    int i;
    int found = -1;
    for (i = 0; i < n; i++) {
        if (prog[i].label[0] != '\0' && strcmp(prog[i].label, name) == 0) {
            if (found >= 0) {
                return -2;
            }
            found = i;
        }
    }
    return found;
}

static int resolve(X86Insn *prog, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int at;
        prog[i].addr = 0x00401000u + (uint32_t)i * 4u;
        if (prog[i].op != X_JUMP && prog[i].op != X_CALL) {
            continue;
        }
        at = find_label(prog, n, prog[i].tname);
        if (at < 0) {
            return -1;
        }
        prog[i].target = at;
    }
    return 0;
}

int x86_load(const char *const *lines, int n, X86Insn *prog, int *count) {
    char pending[20] = "";
    int i;
    int got = 0;
    if (n < 1 || n > X86_INS) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        char buf[96];
        char *tok[8];
        int nt;
        int labeled;
        X86Insn ins;
        memset(&ins, 0, sizeof ins);
        ins.rd = ins.rs = ins.mem_reg = -1;
        ins.target = -1;
        snprintf(buf, sizeof buf, "%s", lines[i]);
        snprintf(ins.text, sizeof ins.text, "%s", lines[i]);
        nt = num_split(buf, tok, 8);
        if (nt < 1) {
            return -1;
        }
        labeled = take_label(tok[0], pending);
        if (labeled != 0) {
            if (labeled < 0 || nt != 1) {
                return -1;
            }
            continue;
        }
        if (parse_insn(&ins, tok, nt) != 0) {
            fprintf(stderr, "无法解析：%s\n", lines[i]);
            return -1;
        }
        snprintf(ins.label, sizeof ins.label, "%s", pending);
        pending[0] = '\0';
        prog[got++] = ins;
    }
    if (got == 0 || pending[0] != '\0' || resolve(prog, got) != 0) {
        fprintf(stderr, "标签没有对应的指令\n");
        return -1;
    }
    *count = got;
    return 0;
}
