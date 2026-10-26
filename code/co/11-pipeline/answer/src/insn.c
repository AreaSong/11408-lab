#include "insn.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct RegTab {
    char name[INSN_N][INSN_NAME];
    int n;
} RegTab;

typedef struct LabTab {
    char name[INSN_N][16];
    int index[INSN_N];
    int n;
} LabTab;

static void set_err(char *err, int errlen, const char *msg) {
    if (err != NULL && errlen > 0) {
        snprintf(err, (size_t)errlen, "%s", msg);
    }
}

static int same_name(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int reg_id(RegTab *tab, const char *name, char *err, int errlen) {
    int i;
    if (strlen(name) >= INSN_NAME) {
        set_err(err, errlen, "寄存器名字太长");
        return -1;
    }
    for (i = 0; i < tab->n; i++) {
        if (same_name(tab->name[i], name)) {
            return i;
        }
    }
    if (tab->n >= INSN_N) {
        set_err(err, errlen, "寄存器太多");
        return -1;
    }
    snprintf(tab->name[tab->n], INSN_NAME, "%s", name);
    return tab->n++;
}

static int is_reg(const char *s) {
    if (!isalpha((unsigned char)s[0])) {
        return 0;
    }
    return strchr(s, '(') == NULL && strchr(s, '[') == NULL;
}

static int is_imm(const char *s) {
    if (*s == '-' || *s == '+') {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0') {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}

static int add_ex(Insn *ins, int reg, const char *name) {
    int i;
    if (reg < 0) {
        return 0;
    }
    for (i = 0; i < ins->nex; i++) {
        if (ins->ex[i] == reg) {
            return 0;
        }
    }
    if (ins->nex >= 2) {
        return -1;
    }
    ins->ex[ins->nex] = reg;
    snprintf(ins->ex_name[ins->nex], INSN_NAME, "%s", name);
    ins->nex++;
    return 0;
}

static int add_memsrc(Insn *ins, int reg, const char *name) {
    if (reg < 0) {
        return 0;
    }
    if (ins->nmem >= 1) {
        return -1;
    }
    ins->memsrc[0] = reg;
    snprintf(ins->mem_name, INSN_NAME, "%s", name);
    ins->nmem = 1;
    return 0;
}

static int mem_base(const char *tok, char *base, int blen) {
    const char *left = strchr(tok, '(');
    const char *right;
    int n;
    base[0] = '\0';
    if (left == NULL) {
        return 0;
    }
    right = strchr(left, ')');
    if (right == NULL || right == left + 1) {
        return -1;
    }
    n = (int)(right - left - 1);
    if (n >= blen) {
        return -1;
    }
    memcpy(base, left + 1, (size_t)n);
    base[n] = '\0';
    return 1;
}

static int op_is(const char *op, const char *name) {
    return same_name(op, name);
}

static int parse_alu3(Insn *ins, char **tok, int nt, RegTab *tab, char *err, int errlen) {
    int rd;
    int rs;
    int rt;
    if (nt == 4 && is_reg(tok[1]) && is_reg(tok[2]) && is_reg(tok[3])) {
        rd = reg_id(tab, tok[1], err, errlen);
        rs = reg_id(tab, tok[2], err, errlen);
        rt = reg_id(tab, tok[3], err, errlen);
        if (rd < 0 || rs < 0 || rt < 0) {
            return -1;
        }
        ins->rd = rd;
        if (add_ex(ins, rs, tok[2]) != 0 || add_ex(ins, rt, tok[3]) != 0) {
            set_err(err, errlen, "源寄存器超过两个");
            return -1;
        }
        return 0;
    }
    if (nt == 4 && is_reg(tok[1]) && is_reg(tok[2]) && is_imm(tok[3])) {
        rd = reg_id(tab, tok[1], err, errlen);
        rs = reg_id(tab, tok[2], err, errlen);
        if (rd < 0 || rs < 0) {
            return -1;
        }
        ins->rd = rd;
        return add_ex(ins, rs, tok[2]);
    }
    set_err(err, errlen, "运算指令写成 add rd, rs, rt 或 add rd, rs, 立即数");
    return -1;
}

/* 2012 的 ADD Rs, Rd 是 (Rs)+(Rd)→Rd，目的寄存器在后面。 */
static int parse_add2(Insn *ins, char **tok, RegTab *tab, char *err, int errlen) {
    int rs = reg_id(tab, tok[1], err, errlen);
    int rd = reg_id(tab, tok[2], err, errlen);
    if (rs < 0 || rd < 0) {
        return -1;
    }
    ins->rd = rd;
    if (add_ex(ins, rs, tok[1]) != 0 || add_ex(ins, rd, tok[2]) != 0) {
        set_err(err, errlen, "源寄存器超过两个");
        return -1;
    }
    return 0;
}

static int parse_load(Insn *ins, char **tok, int nt, RegTab *tab, char *err, int errlen) {
    char base[INSN_NAME];
    int has;
    int rd;
    int rs;
    if (nt != 3 || !is_reg(tok[1])) {
        set_err(err, errlen, "load 写成 load rd, [x] 或 load rd, 0(rs)");
        return -1;
    }
    rd = reg_id(tab, tok[1], err, errlen);
    if (rd < 0) {
        return -1;
    }
    has = mem_base(tok[2], base, INSN_NAME);
    if (has < 0) {
        set_err(err, errlen, "load 的括号没有配好");
        return -1;
    }
    ins->rd = rd;
    ins->uses_mem = 1;
    ins->is_load = 1;
    if (has == 1) {
        rs = reg_id(tab, base, err, errlen);
        if (rs < 0 || add_ex(ins, rs, base) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_store(Insn *ins, char **tok, int nt, RegTab *tab, char *err, int errlen) {
    char base[INSN_NAME];
    int has;
    int data;
    int rs;
    if (nt != 3 || !is_reg(tok[1])) {
        set_err(err, errlen, "store 写成 store rs, [x] 或 store rs, 0(rt)");
        return -1;
    }
    data = reg_id(tab, tok[1], err, errlen);
    if (data < 0) {
        return -1;
    }
    has = mem_base(tok[2], base, INSN_NAME);
    if (has < 0) {
        set_err(err, errlen, "store 的括号没有配好");
        return -1;
    }
    ins->rd = -1;
    ins->uses_mem = 1;
    if (add_memsrc(ins, data, tok[1]) != 0) {
        return -1;
    }
    if (has == 1) {
        rs = reg_id(tab, base, err, errlen);
        if (rs < 0 || add_ex(ins, rs, base) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_branch(Insn *ins, char **tok, int nt, RegTab *tab, char *err, int errlen) {
    int rs;
    int rt;
    if (nt != 4 || !is_reg(tok[1]) || !is_reg(tok[2])) {
        set_err(err, errlen, "分支写成 beq rs, rt, 标号；不转移时标号写 not");
        return -1;
    }
    rs = reg_id(tab, tok[1], err, errlen);
    rt = reg_id(tab, tok[2], err, errlen);
    if (rs < 0 || rt < 0) {
        return -1;
    }
    ins->is_branch = 1;
    ins->rd = -1;
    ins->target = -1;
    if (add_ex(ins, rs, tok[1]) != 0 || add_ex(ins, rt, tok[2]) != 0) {
        return -1;
    }
    if (same_name(tok[3], "not")) {
        ins->taken = 0;
        return 0;
    }
    ins->taken = 1;
    /* 标号先记在 text 之外，用 target 的负数占位，第二遍再填。名字暂存在 mem_name 不会被 store 使用。 */
    if (strlen(tok[3]) >= INSN_NAME) {
        set_err(err, errlen, "标号太长");
        return -1;
    }
    snprintf(ins->mem_name, INSN_NAME, "%s", tok[3]);
    return 0;
}

static int parse_shift(Insn *ins, char **tok, int nt, RegTab *tab, char *err, int errlen) {
    int rd;
    int rs;
    if (op_is(tok[0], "shl")) {
        if (nt != 2 || !is_reg(tok[1])) {
            set_err(err, errlen, "shl 写成 shl rd");
            return -1;
        }
        rd = reg_id(tab, tok[1], err, errlen);
        if (rd < 0) {
            return -1;
        }
        ins->rd = rd;
        return add_ex(ins, rd, tok[1]);
    }
    if (nt != 4 || !is_reg(tok[1]) || !is_reg(tok[2]) || !is_imm(tok[3])) {
        set_err(err, errlen, "sll 写成 sll rd, rs, 立即数");
        return -1;
    }
    rd = reg_id(tab, tok[1], err, errlen);
    rs = reg_id(tab, tok[2], err, errlen);
    if (rd < 0 || rs < 0) {
        return -1;
    }
    ins->rd = rd;
    return add_ex(ins, rs, tok[2]);
}

static int parse_op(Insn *ins, char **tok, int nt, RegTab *tab, char *err, int errlen) {
    const char *op = tok[0];
    if (op_is(op, "nop")) {
        if (nt != 1) {
            set_err(err, errlen, "nop 不带操作数");
            return -1;
        }
        return 0;
    }
    if (op_is(op, "shl") || op_is(op, "sll")) {
        return parse_shift(ins, tok, nt, tab, err, errlen);
    }
    if (op_is(op, "load")) {
        return parse_load(ins, tok, nt, tab, err, errlen);
    }
    if (op_is(op, "store")) {
        return parse_store(ins, tok, nt, tab, err, errlen);
    }
    if (op_is(op, "beq") || op_is(op, "bne")) {
        return parse_branch(ins, tok, nt, tab, err, errlen);
    }
    if (op_is(op, "add") && nt == 3 && is_reg(tok[1]) && is_reg(tok[2])) {
        return parse_add2(ins, tok, tab, err, errlen);
    }
    if (op_is(op, "add") || op_is(op, "sub") || op_is(op, "and") || op_is(op, "or") ||
        op_is(op, "addi")) {
        return parse_alu3(ins, tok, nt, tab, err, errlen);
    }
    set_err(err, errlen, "不认识的指令");
    return -1;
}

static int split_tok(char *s, char **tok, int cap) {
    int n = 0;
    while (*s != '\0' && n < cap) {
        while (*s == ' ' || *s == '\t' || *s == ',') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        tok[n++] = s;
        while (*s != '\0' && *s != ' ' && *s != '\t' && *s != ',') {
            s++;
        }
        if (*s != '\0') {
            *s++ = '\0';
        }
    }
    return n;
}

static int take_label(char *body, char *label, int lablen) {
    char *colon = strchr(body, ':');
    char *p;
    if (colon == NULL || colon == body) {
        return 0;
    }
    for (p = body; p < colon; p++) {
        if (*p == ' ' || *p == '\t') {
            return 0;
        }
    }
    if ((int)(colon - body) >= lablen) {
        return -1;
    }
    memcpy(label, body, (size_t)(colon - body));
    label[colon - body] = '\0';
    memmove(body, colon + 1, strlen(colon + 1) + 1);
    return 1;
}

static int find_lab(const LabTab *labs, const char *name) {
    int i;
    for (i = 0; i < labs->n; i++) {
        if (same_name(labs->name[i], name)) {
            return labs->index[i];
        }
    }
    return -1;
}

static void clear_insn(Insn *ins) {
    memset(ins, 0, sizeof *ins);
    ins->rd = -1;
    ins->target = -1;
}

static const char *cut_line(const char **p, char *buf, int cap, char *err, int errlen) {
    const char *nl = strchr(*p, '\n');
    int len = nl == NULL ? (int)strlen(*p) : (int)(nl - *p);
    if (len >= cap) {
        set_err(err, errlen, "有一行太长");
        return NULL;
    }
    memcpy(buf, *p, (size_t)len);
    buf[len] = '\0';
    if (len > 0 && buf[len - 1] == '\r') {
        buf[len - 1] = '\0';
    }
    *p = nl == NULL ? *p + len : nl + 1;
    return buf;
}

static int remember_label(LabTab *labs, const char *label, int index, char *err, int errlen) {
    if (labs->n >= INSN_N) {
        set_err(err, errlen, "标号太多");
        return -1;
    }
    snprintf(labs->name[labs->n], sizeof labs->name[labs->n], "%s", label);
    labs->index[labs->n] = index;
    labs->n++;
    return 0;
}

static void bind_targets(Insn *out, int n, const LabTab *labs) {
    int i;
    for (i = 0; i < n; i++) {
        if (!out[i].is_branch || !out[i].taken || out[i].mem_name[0] == '\0') {
            continue;
        }
        /* mem_name 暂存标号。分支不读存储器，绑完就清掉。 */
        out[i].target = find_lab(labs, out[i].mem_name);
        out[i].mem_name[0] = '\0';
    }
}

int insn_parse(const char *src, Insn *out, int cap, char *err, int errlen) {
    RegTab regs;
    LabTab labs;
    char buf[80];
    const char *p = src;
    int n = 0;
    if (err != NULL && errlen > 0) {
        err[0] = '\0';
    }
    memset(&regs, 0, sizeof regs);
    memset(&labs, 0, sizeof labs);
    while (*p != '\0') {
        char label[16];
        char raw[80];
        char *tok[6];
        int got;
        int nt;
        if (cut_line(&p, buf, (int)sizeof buf, err, errlen) == NULL) {
            return -1;
        }
        snprintf(raw, sizeof raw, "%s", buf);
        got = take_label(buf, label, (int)sizeof label);
        if (got < 0) {
            set_err(err, errlen, "标号太长");
            return -1;
        }
        nt = split_tok(buf, tok, 6);
        if (nt == 0) {
            continue;
        }
        if (n >= cap) {
            set_err(err, errlen, "指令太多");
            return -1;
        }
        clear_insn(&out[n]);
        snprintf(out[n].text, sizeof out[n].text, "%s", raw);
        if (parse_op(&out[n], tok, nt, &regs, err, errlen) != 0) {
            if (err != NULL && err[0] == '\0') {
                set_err(err, errlen, "指令写错了");
            }
            return -1;
        }
        if (got && remember_label(&labs, label, n, err, errlen) != 0) {
            return -1;
        }
        n++;
    }
    bind_targets(out, n, &labs);
    return n;
}

int insn_raw(const Insn *ins, int earlier, int later) {
    int k;
    int rd;
    if (earlier >= later || ins[earlier].rd < 0) {
        return 0;
    }
    rd = ins[earlier].rd;
    for (k = 0; k < ins[later].nex; k++) {
        if (ins[later].ex[k] == rd) {
            return 1;
        }
    }
    for (k = 0; k < ins[later].nmem; k++) {
        if (ins[later].memsrc[k] == rd) {
            return 1;
        }
    }
    return 0;
}
