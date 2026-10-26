#include "ctrl.h"

#include <stdio.h>

int ceil_log2(int n) {
    unsigned v = 1;
    int bits = 0;
    if (n <= 1) {
        return 0;
    }
    while (v < (unsigned)n) {
        v <<= 1;
        bits++;
    }
    return bits;
}

/* 字段直接编码要留一个「本字段不发微命令」的状态，所以是 n+1 而不是 n。 */
int field_bits(int commands) {
    return ceil_log2(commands + 1);
}

int out_count(uint32_t sig) {
    int n = 0;
    uint32_t outs = sig & SIG_OUTS;
    while (outs != 0) {
        n += (int)(outs & 1u);
        outs >>= 1;
    }
    return n;
}

const char *op_name(int op) {
    static const char *const names[] = {
        "NOP", "LDA", "STA", "ADD", "DIV", "JMP", "TRAP", "HLT", "IRET", "FAIL"
    };
    if (op < 0 || op > OP_FAIL) {
        return "???";
    }
    return names[op];
}

static const char *sig_name(int bit) {
    static const char *const names[SIG_N] = {
        "PCout", "MARin", "MemR", "MDRinE", "PC+1",
        "MDRout", "IRin", "IRout", "ACCin", "ACCout",
        "Ain", "Add", "Div", "MDRin", "MDRoutE",
        "MemW", "PCin", "SavePC", "IFclr", "Vec",
        "SPCout", "IFset"
    };
    return names[bit];
}

/* 打印按「先输出、再运算、再打入」的习惯，不按微指令里的位号。 */
static const int k_print_order[SIG_N] = {
    0, 5, 7, 9, 20, 11, 12, 1, 6, 8, 10, 13, 16, 2, 3, 14, 15, 4, 17, 18, 19, 21
};

void format_sigs(char *buf, size_t n, uint32_t sig) {
    size_t used = 0;
    int bit;
    if (n == 0) {
        return;
    }
    buf[0] = '\0';
    if (sig == 0) {
        snprintf(buf, n, "无");
        return;
    }
    for (bit = 0; bit < SIG_N; bit++) {
        int index = k_print_order[bit];
        int wrote;
        if ((sig & (1u << index)) == 0) {
            continue;
        }
        wrote = snprintf(buf + used, n - used, "%s%s", used ? " " : "", sig_name(index));
        if (wrote < 0 || (size_t)wrote >= n - used) {
            buf[n - 1] = '\0';
            return;
        }
        used += (size_t)wrote;
    }
}

void format_inst(char *buf, size_t n, uint16_t word) {
    int op = (int)(word >> 12);
    int arg = (int)(word & 0xFF);
    if (op == OP_NOP || op == OP_HLT || op == OP_TRAP || op == OP_IRET || op == OP_FAIL) {
        snprintf(buf, n, "%s", op_name(op));
        return;
    }
    snprintf(buf, n, "%s %d", op_name(op), arg);
}
