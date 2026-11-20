#include "asm/asm_internal.h"

#include <ctype.h>
#include <string.h>

char *asm_trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return s;
    }
    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) {
        *end-- = '\0';
    }
    return s;
}

int asm_copy_code(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    int quote = 0;
    while (*src != '\0' && n + 1 < cap) {
        if (*src == '"') {
            quote = !quote;
        }
        if (*src == ';' && !quote) {
            break;
        }
        dst[n++] = *src++;
    }
    if (*src != '\0' && *src != ';') {
        return -1;
    }
    dst[n] = '\0';
    return 0;
}

static void lower_copy(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

int asm_is_reg(const char *text, int *reg) {
    char name[8];
    lower_copy(name, sizeof name, text);
    if (name[0] == 'r' && name[1] >= '0' && name[1] <= '7' && name[2] == '\0') {
        *reg = name[1] - '0';
        return 1;
    }
    if (strcmp(name, "fp") == 0) {
        *reg = 5;
        return 1;
    }
    if (strcmp(name, "lr") == 0) {
        *reg = 6;
        return 1;
    }
    if (strcmp(name, "sp") == 0) {
        *reg = 7;
        return 1;
    }
    return 0;
}

int asm_split(const char *args, char op[][80], int max) {
    int n = 0;
    const char *p = args;
    while (*p != '\0') {
        const char *start;
        int depth = 0;
        size_t len;
        char buf[80];
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (n >= max) {
            return -1;
        }
        start = p;
        while (*p != '\0' && !(depth == 0 && *p == ',')) {
            if (*p == '[') {
                depth++;
            } else if (*p == ']' && depth > 0) {
                depth--;
            }
            p++;
        }
        len = (size_t)(p - start);
        if (len >= sizeof buf) {
            return -1;
        }
        memcpy(buf, start, len);
        buf[len] = '\0';
        snprintf(op[n], 80, "%s", asm_trim(buf));
        n++;
        if (*p == ',') {
            p++;
        }
    }
    return n;
}

static int is_ident(const char *text) {
    int i;
    if (!(isalpha((unsigned char)text[0]) || text[0] == '_')) {
        return 0;
    }
    for (i = 1; text[i] != '\0'; i++) {
        if (!(isalnum((unsigned char)text[i]) || text[i] == '_')) {
            return 0;
        }
    }
    return 1;
}

static int lookup(const SymTab *tab, const char *name, int64_t *out) {
    int i;
    if (tab == NULL) {
        return -1;
    }
    for (i = 0; i < tab->n; i++) {
        if (strcmp(tab->item[i].name, name) == 0) {
            *out = tab->item[i].addr;
            return 0;
        }
    }
    return -1;
}

int asm_atom(const char *tok, const SymTab *tab, int64_t *out, char *err, size_t cap) {
    if (tok[0] == '\0') {
        mm_err(err, cap, "缺少操作数");
        return -1;
    }
    if (isdigit((unsigned char)tok[0]) || tok[0] == '+' || tok[0] == '-' ||
        (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))) {
        return mm_parse_i64(tok, out, err, cap);
    }
    if (!is_ident(tok)) {
        mm_err(err, cap, "无法识别 %s", tok);
        return -1;
    }
    if (lookup(tab, tok, out) != 0) {
        mm_err(err, cap, "未定义的名字 %s", tok);
        return -1;
    }
    return 0;
}

static int squash(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    while (*src != '\0') {
        if (*src != ' ' && *src != '\t') {
            if (n + 1 >= cap) {
                return -1;
            }
            dst[n++] = *src;
        }
        src++;
    }
    dst[n] = '\0';
    return 0;
}

static int take_reg(const char *text, int *reg, const char **rest) {
    char buf[8];
    size_t n = 0;
    while (text[n] != '\0' && (isalnum((unsigned char)text[n]) || text[n] == '_') && n < 7) {
        buf[n] = text[n];
        n++;
    }
    buf[n] = '\0';
    if (!asm_is_reg(buf, reg)) {
        return -1;
    }
    *rest = text + n;
    return 0;
}

static int parse_signed(const char *text, const SymTab *tab, int64_t *out, char *err, size_t cap) {
    int neg = 0;
    if (*text == '+' || *text == '-') {
        neg = *text == '-';
        text++;
    }
    if (asm_atom(text, tab, out, err, cap) != 0) {
        return -1;
    }
    if (neg) {
        *out = -*out;
    }
    return 0;
}

static int parse_pc(const char *inner, const SymTab *tab, MemOp *mem, char *err, size_t cap) {
    int rel = inner[2] == '+';
    const char *rest = inner + 3;
    mem->mode = MM_MODE_REL;
    mem->rx = 0;
    if (rest[0] != '\0' && is_ident(rest)) {
        int64_t addr;
        if (!rel || lookup(tab, rest, &addr) != 0) {
            mm_err(err, cap, "相对寻址里的标号 %s 无效", rest);
            return -1;
        }
        mem->rel_target = 1;
        mem->imm = addr;
        return 0;
    }
    if (parse_signed(inner + 2, tab, &mem->imm, err, cap) != 0) {
        return -1;
    }
    return 0;
}

static int parse_bracket(const char *inner, const SymTab *tab, MemOp *mem, char *err, size_t cap) {
    const char *rest = NULL;
    int reg = 0;
    if (strncmp(inner, "pc+", 3) == 0 || strncmp(inner, "pc-", 3) == 0) {
        return parse_pc(inner, tab, mem, err, cap);
    }
    if (take_reg(inner, &reg, &rest) == 0) {
        mem->rx = reg;
        if (*rest == '\0') {
            mem->mode = MM_MODE_REG;
            return 0;
        }
        if (*rest == '+' || *rest == '-') {
            mem->mode = MM_MODE_BASE;
            return parse_signed(rest, tab, &mem->imm, err, cap);
        }
    }
    mem->mode = MM_MODE_DIR;
    return asm_atom(inner, tab, &mem->imm, err, cap);
}

int asm_parse_mem(const char *text, const SymTab *tab, MemOp *mem, char *err, size_t cap) {
    char s[128];
    char *rb;
    memset(mem, 0, sizeof *mem);
    mem->rx = 0;
    if (squash(s, sizeof s, text) != 0) {
        mm_err(err, cap, "寻址方式太长");
        return -1;
    }
    if (s[0] == '#') {
        mem->mode = MM_MODE_IMM;
        return asm_atom(s + 1, tab, &mem->imm, err, cap);
    }
    if (s[0] == '[' && s[1] == '[') {
        size_t n = strlen(s);
        if (n < 4 || s[n - 1] != ']' || s[n - 2] != ']') {
            mm_err(err, cap, "间接寻址缺少 ]]");
            return -1;
        }
        s[n - 2] = '\0';
        mem->mode = MM_MODE_IND;
        return asm_atom(s + 2, tab, &mem->imm, err, cap);
    }
    if (s[0] == '[') {
        size_t n = strlen(s);
        if (n < 3 || s[n - 1] != ']') {
            mm_err(err, cap, "括号不配对");
            return -1;
        }
        s[n - 1] = '\0';
        return parse_bracket(s + 1, tab, mem, err, cap);
    }
    rb = strchr(s, '[');
    if (rb == NULL || rb == s || s[strlen(s) - 1] != ']') {
        mm_err(err, cap, "无法识别寻址方式 %s", text);
        return -1;
    }
    *rb = '\0';
    rb[strlen(rb + 1)] = '\0';
    if (!asm_is_reg(rb + 1, &mem->rx)) {
        mm_err(err, cap, "变址寄存器无效");
        return -1;
    }
    mem->mode = MM_MODE_IDX;
    return asm_atom(s, tab, &mem->imm, err, cap);
}
