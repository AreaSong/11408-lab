#include "asm/asm_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void asm_rapp(AsmCtx *cx, const char *fmt, ...) {
    va_list ap;
    char buf[256];
    size_t n;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    n = strlen(buf);
    if (cx->rlen + n + 1 > cx->rcap) {
        size_t cap = cx->rcap == 0 ? 1024 : cx->rcap * 2;
        char *grown;
        while (cap < cx->rlen + n + 1) {
            cap *= 2;
        }
        grown = realloc(cx->report, cap);
        if (grown == NULL) {
            return;
        }
        cx->report = grown;
        cx->rcap = cap;
    }
    memcpy(cx->report + cx->rlen, buf, n + 1);
    cx->rlen += n;
}

static int add_sym(AsmCtx *cx, const char *name, uint32_t addr, char *err, size_t cap, int lineno) {
    int reg = 0;
    int i;
    if (asm_is_reg(name, &reg) || strcmp(name, "pc") == 0) {
        mm_err(err, cap, "第 %d 行：%s 不能做标号", lineno, name);
        return -1;
    }
    for (i = 0; i < cx->sym.n; i++) {
        if (strcmp(cx->sym.item[i].name, name) == 0) {
            mm_err(err, cap, "第 %d 行：重复定义 %s", lineno, name);
            return -1;
        }
    }
    if (cx->sym.n >= cx->sym.cap) {
        mm_err(err, cap, "标号太多");
        return -1;
    }
    snprintf(cx->sym.item[cx->sym.n].name, 32, "%s", name);
    cx->sym.item[cx->sym.n].addr = addr;
    cx->sym.n++;
    return 0;
}

static int take_label(char *s, ALine *line) {
    char *q = s;
    size_t n;
    if (!(isalpha((unsigned char)*q) || *q == '_')) {
        return 0;
    }
    while (isalnum((unsigned char)*q) || *q == '_') {
        q++;
    }
    if (*q != ':') {
        return 0;
    }
    n = (size_t)(q - s);
    if (n >= sizeof line->label) {
        return -1;
    }
    memcpy(line->label, s, n);
    line->label[n] = '\0';
    return (int)(q - s + 1);
}

int asm_parse_source(const char *raw, int lineno, ALine *line, char *err, size_t cap) {
    char buf[240];
    char *s;
    char *sp;
    int used;
    memset(line, 0, sizeof *line);
    line->lineno = lineno;
    if (asm_copy_code(buf, sizeof buf, raw) != 0) {
        mm_err(err, cap, "第 %d 行：太长", lineno);
        return -1;
    }
    s = asm_trim(buf);
    if (*s == '\0') {
        return 0;
    }
    snprintf(line->show, sizeof line->show, "%s", s);
    used = take_label(s, line);
    if (used < 0) {
        mm_err(err, cap, "第 %d 行：标号太长", lineno);
        return -1;
    }
    s = asm_trim(s + used);
    if (*s == '\0') {
        return 0;
    }
    sp = s;
    while (*sp != '\0' && *sp != ' ' && *sp != '\t') {
        sp++;
    }
    if ((size_t)(sp - s) >= sizeof line->mnem) {
        mm_err(err, cap, "第 %d 行：助记符太长", lineno);
        return -1;
    }
    memcpy(line->mnem, s, (size_t)(sp - s));
    line->mnem[sp - s] = '\0';
    {
        char *m = line->mnem;
        for (; *m != '\0'; m++) {
            *m = (char)tolower((unsigned char)*m);
        }
    }
    snprintf(line->args, sizeof line->args, "%s", asm_trim(sp));
    return 0;
}

static int count_items(const char *args) {
    int n = *args == '\0' ? 0 : 1;
    const char *p = args;
    int quote = 0;
    for (; *p != '\0'; p++) {
        if (*p == '"') {
            quote = !quote;
        }
        if (*p == ',' && !quote) {
            n++;
        }
    }
    return n;
}

static int string_size(const char *args, int asciz, int *size, char *err, size_t cap) {
    const char *p = args;
    int n = 0;
    if (*p != '"') {
        mm_err(err, cap, "字符串要以引号开头");
        return -1;
    }
    for (p++; *p != '\0' && *p != '"'; p++) {
        if (*p == '\\') {
            if (p[1] == '\0') {
                mm_err(err, cap, "字符串没有结束");
                return -1;
            }
            p++;
        }
        n++;
    }
    if (*p != '"') {
        mm_err(err, cap, "字符串没有结束");
        return -1;
    }
    *size = n + (asciz ? 1 : 0);
    return 0;
}

static int power2(int64_t n) {
    return n > 0 && n <= 4096 && (n & (n - 1)) == 0;
}

static int layout_dir(AsmCtx *cx, ALine *line, uint32_t *lc, char *err, size_t cap) {
    int64_t val = 0;
    const char *name = line->mnem;
    if (strcmp(name, ".text") == 0 || strcmp(name, ".data") == 0) {
        return 0;
    }
    if (strcmp(name, ".org") == 0) {
        if (asm_atom(line->args, &cx->sym, &val, err, cap) != 0 || val < 0 || val >= MM_MEM_MAX) {
            mm_err(err, cap, "第 %d 行：.org 地址无效", line->lineno);
            return -1;
        }
        *lc = (uint32_t)val;
        return 0;
    }
    if (strcmp(name, ".equ") == 0) {
        char op[4][80];
        int n = asm_split(line->args, op, 4);
        if (n != 2 || asm_atom(op[1], &cx->sym, &val, err, cap) != 0 || val < 0 || val > 4294967295LL) {
            mm_err(err, cap, "第 %d 行：.equ 需要 名字, 值", line->lineno);
            return -1;
        }
        return add_sym(cx, op[0], (uint32_t)val, err, cap, line->lineno);
    }
    if (strcmp(name, ".align") == 0) {
        uint32_t aligned;
        if (asm_atom(line->args, &cx->sym, &val, err, cap) != 0 || !power2(val)) {
            mm_err(err, cap, "第 %d 行：.align 要是 2 的幂", line->lineno);
            return -1;
        }
        aligned = (*lc + (uint32_t)val - 1u) & ~((uint32_t)val - 1u);
        line->kind = LK_SPACE;
        line->addr = *lc;
        line->nbytes = (int)(aligned - *lc);
        *lc = aligned;
        return 0;
    }
    if (strcmp(name, ".space") == 0) {
        if (asm_atom(line->args, &cx->sym, &val, err, cap) != 0 || val < 0) {
            mm_err(err, cap, "第 %d 行：.space 无效", line->lineno);
            return -1;
        }
        line->kind = LK_SPACE;
        line->addr = *lc;
        line->nbytes = (int)val;
        *lc += (uint32_t)val;
        return 0;
    }
    if (strcmp(name, ".word") == 0) {
        int n = count_items(line->args);
        if (n <= 0) {
            mm_err(err, cap, "第 %d 行：.word 没有数据", line->lineno);
            return -1;
        }
        if ((*lc & 3u) != 0) {
            mm_err(err, cap, "第 %d 行：.word 地址未对齐", line->lineno);
            return -1;
        }
        line->kind = LK_WORD;
        line->addr = *lc;
        line->nbytes = n * 4;
        *lc += (uint32_t)line->nbytes;
        return 0;
    }
    if (strcmp(name, ".byte") == 0) {
        int n = count_items(line->args);
        if (n <= 0) {
            mm_err(err, cap, "第 %d 行：.byte 没有数据", line->lineno);
            return -1;
        }
        line->kind = LK_BYTE;
        line->addr = *lc;
        line->nbytes = n;
        *lc += (uint32_t)n;
        return 0;
    }
    if (strcmp(name, ".ascii") == 0 || strcmp(name, ".asciz") == 0) {
        int size = 0;
        line->asciz = name[5] == 'z';
        if (string_size(line->args, line->asciz, &size, err, cap) != 0) {
            char why[160];
            snprintf(why, sizeof why, "%s", err);
            mm_err(err, cap, "第 %d 行：%s", line->lineno, why);
            return -1;
        }
        line->kind = LK_STR;
        line->addr = *lc;
        line->nbytes = size;
        *lc += (uint32_t)size;
        return 0;
    }
    mm_err(err, cap, "第 %d 行：未知伪操作 %s", line->lineno, name);
    return -1;
}

int asm_layout(AsmCtx *cx, char *err, size_t cap) {
    uint32_t lc = 0x00001000u;
    int i;
    cx->entry = lc;
    for (i = 0; i < cx->nline; i++) {
        ALine *line = &cx->line[i];
        int width;
        if (line->label[0] != '\0' && add_sym(cx, line->label, lc, err, cap, line->lineno) != 0) {
            return -1;
        }
        if (line->mnem[0] == '\0') {
            continue;
        }
        if (line->mnem[0] == '.') {
            if (layout_dir(cx, line, &lc, err, cap) != 0) {
                return -1;
            }
            continue;
        }
        width = asm_width(line->mnem, line->args, &cx->sym, err, cap);
        if (width < 0) {
            char why[160];
            snprintf(why, sizeof why, "%s", err);
            mm_err(err, cap, "第 %d 行：%s", line->lineno, why);
            return -1;
        }
        if ((lc & 3u) != 0 || lc > MM_MEM_MAX - (uint32_t)width) {
            mm_err(err, cap, "第 %d 行：指令地址未对齐或超出 1MB", line->lineno);
            return -1;
        }
        if (!cx->seen_insn) {
            cx->entry = lc;
            cx->seen_insn = 1;
        }
        line->kind = LK_INSN;
        line->addr = lc;
        line->nbytes = width;
        lc += (uint32_t)width;
    }
    return 0;
}
