#include "asm/asm_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int place(AsmCtx *cx, uint32_t addr, const uint8_t *bytes, int n, int lineno, char *err, size_t cap) {
    int i;
    if (addr > MM_MEM_MAX || n < 0 || (uint32_t)n > MM_MEM_MAX - addr) {
        mm_err(err, cap, "第 %d 行：地址超出 1MB", lineno);
        return -1;
    }
    for (i = 0; i < n; i++) {
        uint32_t at = addr + (uint32_t)i;
        if (cx->mark[at]) {
            mm_err(err, cap, "第 %d 行：地址 %08X 重叠", lineno, at);
            return -1;
        }
    }
    for (i = 0; i < n; i++) {
        cx->mem[addr + (uint32_t)i] = bytes[i];
        cx->mark[addr + (uint32_t)i] = 1;
    }
    cx->nbytes += (uint32_t)n;
    return 0;
}

static void put_word(uint8_t *dst, uint32_t word) {
    dst[0] = (uint8_t)word;
    dst[1] = (uint8_t)(word >> 8);
    dst[2] = (uint8_t)(word >> 16);
    dst[3] = (uint8_t)(word >> 24);
}

static int emit_insn(AsmCtx *cx, const ALine *line, char *err, size_t cap) {
    EncOut enc;
    int i;
    if (asm_encode(line->mnem, line->args, line->addr, &cx->sym, &enc, err, cap) != 0) {
        char why[160];
        snprintf(why, sizeof why, "%s", err);
        mm_err(err, cap, "第 %d 行：%s", line->lineno, why);
        return -1;
    }
    if (enc.n * 4 != line->nbytes) {
        mm_err(err, cap, "第 %d 行：指令长度和第一遍不一致", line->lineno);
        return -1;
    }
    for (i = 0; i < enc.n; i++) {
        uint8_t bytes[4];
        char text[120];
        put_word(bytes, enc.w[i]);
        if (place(cx, line->addr + (uint32_t)i * 4u, bytes, 4, line->lineno, err, cap) != 0) {
            return -1;
        }
        if (i == 0 && line->label[0] != '\0') {
            snprintf(text, sizeof text, "%s: %s", line->label, enc.text[i]);
        } else {
            snprintf(text, sizeof text, "%s", enc.text[i]);
        }
        asm_rapp(cx, "%08X  %08X  %s\n", line->addr + (uint32_t)i * 4u, enc.w[i], text);
    }
    return 0;
}

static int next_item(const char **pp, char *tok, size_t cap) {
    const char *p = *pp;
    size_t n = 0;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0') {
        return 0;
    }
    while (*p != '\0' && *p != ',') {
        if (n + 1 < cap) {
            tok[n++] = *p;
        }
        p++;
    }
    tok[n] = '\0';
    if (*p == ',') {
        p++;
    }
    *pp = p;
    return 1;
}

static int emit_words(AsmCtx *cx, const ALine *line, char *err, size_t cap) {
    const char *p = line->args;
    int i = 0;
    char tok[80];
    while (next_item(&p, tok, sizeof tok)) {
        int64_t val = 0;
        uint32_t bits;
        uint8_t bytes[4];
        char *item = asm_trim(tok);
        if (asm_atom(item, &cx->sym, &val, err, cap) != 0 || val < (int64_t)INT32_MIN || val > 4294967295LL) {
            mm_err(err, cap, "第 %d 行：.word 的值无效", line->lineno);
            return -1;
        }
        bits = (uint32_t)val;
        put_word(bytes, bits);
        if (place(cx, line->addr + (uint32_t)i * 4u, bytes, 4, line->lineno, err, cap) != 0) {
            return -1;
        }
        asm_rapp(cx, "%08X  %08X  %s\n", line->addr + (uint32_t)i * 4u, bits, i == 0 ? line->show : "");
        i++;
    }
    return i == 0 ? -1 : 0;
}

static int byte_value(const char *tok, const SymTab *tab, int *out, char *err, size_t cap) {
    int64_t val = 0;
    if (tok[0] == '\'') {
        unsigned char ch;
        if (tok[1] == '\\') {
            char esc = tok[2];
            if (esc == 'n') ch = '\n';
            else if (esc == 't') ch = '\t';
            else if (esc == '0') ch = 0;
            else if (esc == '\\' || esc == '\'') ch = (unsigned char)esc;
            else {
                mm_err(err, cap, "不支持的转义");
                return -1;
            }
            if (tok[3] != '\'' || tok[4] != '\0') {
                mm_err(err, cap, "字符常量格式错误");
                return -1;
            }
        } else {
            ch = (unsigned char)tok[1];
            if (tok[2] != '\'' || tok[3] != '\0') {
                mm_err(err, cap, "字符常量格式错误");
                return -1;
            }
        }
        *out = ch;
        return 0;
    }
    if (asm_atom(tok, tab, &val, err, cap) != 0 || val < 0 || val > 255) {
        mm_err(err, cap, "字节要在 0 到 255");
        return -1;
    }
    *out = (int)val;
    return 0;
}

static int fill_string(const char *args, int asciz, uint8_t *dst, int cap, char *err, size_t errcap) {
    const char *p = args + 1;
    int n = 0;
    while (*p != '\0' && *p != '"') {
        unsigned char ch = (unsigned char)*p;
        if (*p == '\\') {
            p++;
            if (*p == 'n') ch = '\n';
            else if (*p == 't') ch = '\t';
            else if (*p == 'r') ch = '\r';
            else if (*p == '0') ch = 0;
            else ch = (unsigned char)*p;
        }
        if (n >= cap) {
            mm_err(err, errcap, "字符串太长");
            return -1;
        }
        dst[n++] = ch;
        p++;
    }
    if (asciz) {
        if (n >= cap) {
            return -1;
        }
        dst[n++] = 0;
    }
    return n;
}

static int emit_bytes(AsmCtx *cx, const ALine *line, char *err, size_t cap) {
    char op[16][80];
    int n = asm_split(line->args, op, 16);
    int i;
    if (n < 0) {
        mm_err(err, cap, "第 %d 行：.byte 项太多", line->lineno);
        return -1;
    }
    for (i = 0; i < n; i++) {
        int val = 0;
        uint8_t b;
        if (byte_value(op[i], &cx->sym, &val, err, cap) != 0) {
            mm_err(err, cap, "第 %d 行：.byte 的值无效", line->lineno);
            return -1;
        }
        b = (uint8_t)val;
        if (place(cx, line->addr + (uint32_t)i, &b, 1, line->lineno, err, cap) != 0) {
            return -1;
        }
        asm_rapp(cx, "%08X  %02X  %s\n", line->addr + (uint32_t)i, b, i == 0 ? line->show : "");
    }
    return 0;
}

static int emit_str(AsmCtx *cx, const ALine *line, char *err, size_t cap) {
    uint8_t buf[180];
    int n = fill_string(line->args, line->asciz, buf, (int)sizeof buf, err, cap);
    int i;
    if (n < 0 || n != line->nbytes) {
        mm_err(err, cap, "第 %d 行：字符串和第一遍不一致", line->lineno);
        return -1;
    }
    if (place(cx, line->addr, buf, n, line->lineno, err, cap) != 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        asm_rapp(cx, "%08X  %02X  %s\n", line->addr + (uint32_t)i, buf[i], i == 0 ? line->show : "");
    }
    return 0;
}

static int emit_space(AsmCtx *cx, const ALine *line, char *err, size_t cap) {
    uint8_t *zeros;
    if (line->nbytes == 0) {
        return 0;
    }
    zeros = calloc((size_t)line->nbytes, 1);
    if (zeros == NULL || place(cx, line->addr, zeros, line->nbytes, line->lineno, err, cap) != 0) {
        free(zeros);
        return -1;
    }
    free(zeros);
    asm_rapp(cx, "%08X  (%d 字节 0)  %s\n", line->addr, line->nbytes, line->show);
    return 0;
}

static int cmp_sym(const void *a, const void *b) {
    return strcmp(((const AsmSym *)a)->name, ((const AsmSym *)b)->name);
}

static void report_tail(AsmCtx *cx) {
    int i;
    if (cx->sym.n == 0) {
        asm_rapp(cx, "【符号】\n无\n");
    } else {
        qsort(cx->sym.item, (size_t)cx->sym.n, sizeof cx->sym.item[0], cmp_sym);
        asm_rapp(cx, "【符号】\n");
        for (i = 0; i < cx->sym.n; i++) {
            asm_rapp(cx, "%s=%08X\n", cx->sym.item[i].name, cx->sym.item[i].addr);
        }
    }
    asm_rapp(cx, "【汇编】入口=%08X 字节=%u\n", cx->entry, cx->nbytes);
}

static int emit_all(AsmCtx *cx, char *err, size_t cap) {
    int i;
    asm_rapp(cx, "【列表】\n");
    for (i = 0; i < cx->nline; i++) {
        ALine *line = &cx->line[i];
        int rc = 0;
        if (line->kind == LK_INSN) rc = emit_insn(cx, line, err, cap);
        else if (line->kind == LK_WORD) rc = emit_words(cx, line, err, cap);
        else if (line->kind == LK_BYTE) rc = emit_bytes(cx, line, err, cap);
        else if (line->kind == LK_STR) rc = emit_str(cx, line, err, cap);
        else if (line->kind == LK_SPACE) rc = emit_space(cx, line, err, cap);
        if (rc != 0) {
            return -1;
        }
    }
    if (cx->nbytes == 0) {
        mm_err(err, cap, "没有指令或数据");
        return -1;
    }
    report_tail(cx);
    return 0;
}

static int read_lines(const char *src, ALine **out, int *nline, char *err, size_t cap) {
    ALine *lines = calloc(1024, sizeof *lines);
    const char *p = src;
    int n = 0;
    int lineno = 1;
    if (lines == NULL) {
        mm_err(err, cap, "内存不足");
        return -1;
    }
    while (*p != '\0') {
        char buf[240];
        size_t k = 0;
        while (*p != '\0' && *p != '\n' && k + 1 < sizeof buf) {
            if (*p != '\r') {
                buf[k++] = *p;
            }
            p++;
        }
        if (*p != '\0' && *p != '\n') {
            mm_err(err, cap, "第 %d 行：太长", lineno);
            free(lines);
            return -1;
        }
        if (*p == '\n') {
            p++;
        }
        buf[k] = '\0';
        if (n >= 1024) {
            mm_err(err, cap, "源程序超过 1024 行");
            free(lines);
            return -1;
        }
        if (asm_parse_source(buf, lineno, &lines[n], err, cap) != 0) {
            free(lines);
            return -1;
        }
        if (lines[n].mnem[0] != '\0' || lines[n].label[0] != '\0') {
            n++;
        }
        lineno++;
    }
    *out = lines;
    *nline = n;
    return 0;
}

static void ctx_free(AsmCtx *cx) {
    free(cx->sym.item);
    free(cx->line);
    free(cx->mem);
    free(cx->mark);
    free(cx->report);
}

int asm_assemble(const char *src, AsmImage **out, char *err, size_t errcap) {
    AsmCtx cx;
    AsmImage *img;
    memset(&cx, 0, sizeof cx);
    if (err != NULL && errcap > 0) {
        err[0] = '\0';
    }
    cx.sym.cap = 512;
    cx.sym.item = calloc((size_t)cx.sym.cap, sizeof *cx.sym.item);
    cx.mem = calloc(MM_MEM_MAX, 1);
    cx.mark = calloc(MM_MEM_MAX, 1);
    if (cx.sym.item == NULL || cx.mem == NULL || cx.mark == NULL ||
        read_lines(src, &cx.line, &cx.nline, err, errcap) != 0 ||
        asm_layout(&cx, err, errcap) != 0 || emit_all(&cx, err, errcap) != 0) {
        ctx_free(&cx);
        return -1;
    }
    img = calloc(1, sizeof *img);
    if (img == NULL) {
        ctx_free(&cx);
        return -1;
    }
    img->mem = cx.mem;
    img->mark = cx.mark;
    img->space = MM_MEM_MAX;
    img->entry = cx.entry;
    img->nbytes = cx.nbytes;
    img->report = cx.report == NULL ? calloc(1, 1) : cx.report;
    img->sym = cx.sym.item;
    img->nsym = cx.sym.n;
    free(cx.line);
    *out = img;
    return 0;
}

void asm_free(AsmImage *img) {
    if (img == NULL) {
        return;
    }
    free(img->mem);
    free(img->mark);
    free(img->report);
    free(img->sym);
    free(img);
}

const char *asm_report(const AsmImage *img) {
    return img->report == NULL ? "" : img->report;
}

uint32_t asm_entry(const AsmImage *img) {
    return img->entry;
}

int asm_symbol(const AsmImage *img, const char *name, uint32_t *addr) {
    int i;
    for (i = 0; i < img->nsym; i++) {
        if (strcmp(img->sym[i].name, name) == 0) {
            *addr = img->sym[i].addr;
            return 0;
        }
    }
    return -1;
}

int asm_encode_line(const char *line, uint32_t pc, uint32_t *words, int *nwords,
                    char *err, size_t errcap) {
    ALine one;
    EncOut enc;
    SymTab none;
    int i;
    memset(&none, 0, sizeof none);
    if (asm_parse_source(line, 1, &one, err, errcap) != 0) {
        return -1;
    }
    if (one.mnem[0] == '\0' || one.mnem[0] == '.') {
        mm_err(err, errcap, "需要一条指令");
        return -1;
    }
    if (asm_encode(one.mnem, one.args, pc, &none, &enc, err, errcap) != 0) {
        return -1;
    }
    for (i = 0; i < enc.n; i++) {
        words[i] = enc.w[i];
    }
    *nwords = enc.n;
    return 0;
}
