#ifndef ASM_IMAGE_H
#define ASM_IMAGE_H

#include "asm/asm_asm.h"

#include <stdint.h>

typedef struct {
    char name[32];
    uint32_t addr;
} AsmSym;

struct AsmImage {
    uint8_t *mem;
    uint8_t *mark;
    uint32_t space;
    uint32_t entry;
    uint32_t nbytes;
    char *report;
    AsmSym *sym;
    int nsym;
};

#endif
