#ifndef ASM_INTERNAL_H
#define ASM_INTERNAL_H

#include "asm/asm_image.h"
#include "common/mm_internal.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    AsmSym *item;
    int n;
    int cap;
} SymTab;

typedef struct {
    int mode;
    int rx;
    int rel_target;
    int64_t imm;
} MemOp;

typedef struct {
    int n;
    uint32_t w[2];
    char text[2][80];
} EncOut;

typedef struct {
    int lineno;
    char label[32];
    char mnem[24];
    char args[180];
    char show[200];
    uint32_t addr;
    int kind;
    int nbytes;
    int asciz;
} ALine;

enum { LK_NONE = 0, LK_INSN, LK_WORD, LK_BYTE, LK_STR, LK_SPACE };

typedef struct {
    SymTab sym;
    ALine *line;
    int nline;
    uint8_t *mem;
    uint8_t *mark;
    char *report;
    size_t rlen;
    size_t rcap;
    uint32_t entry;
    int seen_insn;
    uint32_t nbytes;
} AsmCtx;

void asm_rapp(AsmCtx *cx, const char *fmt, ...);
int asm_parse_source(const char *raw, int lineno, ALine *line, char *err, size_t cap);
int asm_layout(AsmCtx *cx, char *err, size_t cap);

char *asm_trim(char *s);
int asm_copy_code(char *dst, size_t cap, const char *src);
int asm_is_reg(const char *text, int *reg);
int asm_split(const char *args, char op[][80], int max);
int asm_atom(const char *tok, const SymTab *tab, int64_t *out, char *err, size_t cap);
int asm_parse_mem(const char *text, const SymTab *tab, MemOp *mem, char *err, size_t cap);
int asm_encode(const char *mnem, const char *args, uint32_t pc, const SymTab *tab,
               EncOut *out, char *err, size_t cap);
int asm_width(const char *mnem, const char *args, const SymTab *tab, char *err, size_t cap);

#endif
