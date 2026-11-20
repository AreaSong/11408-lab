#include "cpu/cpu_exec.h"

#include <string.h>

int cpu_ea(uint32_t word, uint32_t pc, uint32_t rx, uint32_t mem_word, CpuEa *out) {
    CpuInsn in;
    int lea;
    memset(out, 0, sizeof *out);
    if (cpu_decode(word, &in) != 0 || in.op < MM_OP_LD || in.op > MM_OP_XCHG) {
        return -1;
    }
    lea = in.op == MM_OP_LEA;
    out->ok = 1;
    out->mode = in.mode;
    switch (in.mode) {
    case MM_MODE_IMM:
        out->mem_ops = 0;
        break;
    case MM_MODE_DIR:
        out->has_ea = 1;
        out->ea = in.imm;
        out->mem_ops = lea ? 0 : 1;
        break;
    case MM_MODE_IND:
        out->has_ea = 1;
        out->ea = mem_word;
        out->mem_ops = lea ? 1 : 2;
        break;
    case MM_MODE_REG:
        out->has_ea = 1;
        out->ea = rx;
        out->mem_ops = lea ? 0 : 1;
        break;
    case MM_MODE_BASE:
        out->has_ea = 1;
        out->ea = rx + (uint32_t)mm_sext16(in.imm);
        out->mem_ops = lea ? 0 : 1;
        break;
    case MM_MODE_IDX:
        out->has_ea = 1;
        out->ea = in.imm + rx;
        out->mem_ops = lea ? 0 : 1;
        break;
    case MM_MODE_REL:
        out->has_ea = 1;
        out->ea = pc + 4u + (uint32_t)mm_sext16(in.imm);
        out->mem_ops = lea ? 0 : 1;
        break;
    default:
        out->ok = 0;
        return -1;
    }
    return 0;
}

int cpu_bus_drivers(const char *signals) {
    int count = 0;
    const char *p = signals;
    if (signals == NULL || strcmp(signals, "无") == 0) {
        return 0;
    }
    while (*p != '\0') {
        const char *start;
        int len;
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ' ') {
            p++;
        }
        len = (int)(p - start);
        if (len >= 3 && start[len - 3] == 'o' && start[len - 2] == 'u' && start[len - 1] == 't') {
            count++;
        }
    }
    return count;
}
