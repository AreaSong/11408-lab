#include "asm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *OP_NAME[] = {"NOP", "LDA", "STA", "ADD", "JMP", "JZ", "ADM", "HLT"};

static int hex_value(const char *text, long *out) {
    unsigned long value = 0;
    if (*text == '\0') {
        return -1;
    }
    while (*text != '\0') {
        int digit;
        if (*text >= '0' && *text <= '9') {
            digit = *text - '0';
        } else if (*text >= 'a' && *text <= 'f') {
            digit = *text - 'a' + 10;
        } else if (*text >= 'A' && *text <= 'F') {
            digit = *text - 'A' + 10;
        } else {
            return -1;
        }
        value = value * 16u + (unsigned)digit;
        if (value > 65535u) {
            return -1;
        }
        text++;
    }
    *out = (long)value;
    return 0;
}

int parse_num(const char *text, long *out) {
    int neg = 0;
    char buf[32];
    size_t n;
    if (text[0] == '-') {
        neg = 1;
        text++;
    }
    n = strlen(text);
    if (n >= sizeof buf) {
        return -1;
    }
    if (n > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        if (hex_value(text + 2, out) != 0) {
            return -1;
        }
    } else if (n > 1 && (text[n - 1] == 'H' || text[n - 1] == 'h')) {
        snprintf(buf, sizeof buf, "%s", text);
        buf[n - 1] = '\0';
        if (hex_value(buf, out) != 0) {
            return -1;
        }
    } else {
        char *end = NULL;
        *out = strtol(text, &end, 10);
        if (end == text || *end != '\0') {
            return -1;
        }
    }
    if (neg) {
        *out = -*out;
    }
    return (*out < -32768 || *out > 65535) ? -1 : 0;
}

static int op_of(const char *name) {
    int i;
    for (i = 0; i < 8; i++) {
        if (strcmp(name, OP_NAME[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static void upper_copy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i + 1 < n && src[i] != '\0'; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

int asm_encode(const char *line, uint16_t *word) {
    char name[8];
    char raw[8];
    int op;
    long operand = 0;
    int got = sscanf(line, "%7s %ld", raw, &operand);
    if (got < 1) {
        return -1;
    }
    upper_copy(name, raw, sizeof name);
    op = op_of(name);
    if (op < 0) {
        return -1;
    }
    if (op == OP_NOP || op == OP_HLT) {
        *word = (uint16_t)(op << 12);
        return got == 1 ? 0 : -1;
    }
    if (got != 2 || operand < 0) {
        return -1;
    }
    if (op == OP_ADM) {
        if (operand > 3) {
            return -1;
        }
        *word = (uint16_t)((op << 12) | (operand << 8));
        return 0;
    }
    if (operand >= MEM_WORDS) {
        return -1;
    }
    *word = (uint16_t)((op << 12) | operand);
    return 0;
}

void fmt_hex(char *buf, size_t n, uint16_t word) {
    snprintf(buf, n, "%04XH", word);
}

void fmt_inst(char *buf, size_t n, uint16_t word) {
    int op = word >> 12;
    int reg = (word >> 8) & 3;
    int addr = word & 0xFF;
    if (op > OP_HLT) {
        snprintf(buf, n, "非法");
    } else if (op == OP_ADM) {
        snprintf(buf, n, "ADM R%d", reg);
    } else if (op == OP_NOP || op == OP_HLT) {
        snprintf(buf, n, "%s", OP_NAME[op]);
    } else {
        snprintf(buf, n, "%s %d", OP_NAME[op], addr);
    }
}
