#include "mem/mem_chip.h"

#include <stdio.h>

static int ceil_log2_u32(uint32_t n) {
    int bits = 0;
    uint32_t value = 1;
    if (n <= 1) {
        return 0;
    }
    while (value < n) {
        if (bits >= 31) {
            return -1;
        }
        value <<= 1;
        bits++;
    }
    return bits;
}

static int exact_log2_u32(uint32_t n) {
    int bits = ceil_log2_u32(n);
    if (bits < 0 || (1u << bits) != n) {
        return -1;
    }
    return bits;
}

static int ext_mode(int groups, int per_group) {
    if (per_group > 1 && groups == 1) {
        return MEM_EXT_BIT;
    }
    if (per_group == 1 && groups > 1) {
        return MEM_EXT_WORD;
    }
    if (per_group > 1 && groups > 1) {
        return MEM_EXT_BOTH;
    }
    return MEM_EXT_NONE;
}

static const char *mode_name(int mode) {
    static const char *names[] = {"位扩展", "字扩展", "字位同时扩展", "不用扩展"};
    if (mode < 0 || mode > 3) {
        return "未知";
    }
    return names[mode];
}

int mem_chip_plan(const MemChipReq *req, MemChipPlan *out) {
    int chip_bits;
    int addr_bits;
    if (req == NULL || out == NULL || req->units == 0 || req->chip_units == 0 || req->word_bits <= 0 ||
        req->chip_bits <= 0) {
        return -1;
    }
    if (req->word_bits % req->chip_bits != 0 || req->units % req->chip_units != 0) {
        return -1;
    }
    chip_bits = exact_log2_u32(req->chip_units);
    addr_bits = exact_log2_u32(req->units);
    if (chip_bits < 0 || addr_bits < 0) {
        return -1;
    }
    out->per_group = req->word_bits / req->chip_bits;
    out->groups = (int)(req->units / req->chip_units);
    out->chips = out->groups * out->per_group;
    out->chip_addr_bits = chip_bits;
    out->select_bits = out->groups <= 1 ? 0 : ceil_log2_u32((uint32_t)out->groups);
    out->addr_bits = addr_bits;
    out->mode = ext_mode(out->groups, out->per_group);
    return out->select_bits < 0 ? -1 : 0;
}

int mem_chip_locate(uint32_t units, uint32_t chip_units, uint32_t addr, MemChipAt *out) {
    int chip_bits = exact_log2_u32(chip_units);
    if (out == NULL || chip_bits < 0 || units == 0 || units % chip_units != 0 || addr >= units) {
        return -1;
    }
    out->group = (int)(addr / chip_units);
    out->min_addr = (uint32_t)out->group * chip_units;
    out->max_addr = out->min_addr + chip_units - 1u;
    out->in_chip = addr & ((1u << chip_bits) - 1u);
    out->select = addr >> chip_bits;
    return 0;
}

void mem_chip_report(FILE *fp, const MemChipReq *req) {
    MemChipPlan plan;
    if (mem_chip_plan(req, &plan) != 0) {
        fprintf(fp, "错误：芯片规格对不齐，单元数和位宽都要能整除，单元数还要是 2 的幂\n");
        return;
    }
    fprintf(fp, "【存储器扩展】目标 %u 个单元 × %d 位，芯片 %u 个单元 × %d 位\n", req->units, req->word_bits,
            req->chip_units, req->chip_bits);
    fprintf(fp, "位扩展拼数据位：每组 %d 片。字扩展靠片选：%d 组\n", plan.per_group, plan.groups);
    fprintf(fp, "片内地址 %d 位，片选 %d 位，CPU 侧地址 %d 位\n", plan.chip_addr_bits, plan.select_bits,
            plan.addr_bits);
    fprintf(fp, "结果：芯片数 = %d，%s，%d 组 × 每组 %d 片\n", plan.chips, mode_name(plan.mode), plan.groups,
            plan.per_group);
}

void mem_chip_locate_report(FILE *fp, uint32_t units, uint32_t chip_units, uint32_t addr) {
    MemChipAt at;
    int select_bits;
    if (mem_chip_locate(units, chip_units, addr, &at) != 0) {
        fprintf(fp, "错误：地址超出这组芯片能覆盖的单元\n");
        return;
    }
    select_bits = units / chip_units <= 1 ? 0 : exact_log2_u32(units / chip_units);
    fprintf(fp, "【片选地址】%u 个单元，一组 %u 个单元。位扩展的几片共用这一段\n", units, chip_units);
    fprintf(fp, "地址 %04XH 在第 %d 组，片选 = %u，片内 = %04XH\n", addr, at.group, at.select, at.in_chip);
    fprintf(fp, "片选 %d 位\n", select_bits < 0 ? 0 : select_bits);
    fprintf(fp, "结果：最小地址 = %04XH，范围 %04XH～%04XH\n", at.min_addr, at.min_addr, at.max_addr);
}
