#include "expand.h"

#include "unit.h"

#include <stdio.h>
#include <string.h>

static int expand_mode(uint64_t groups, int per_group) {
    if (per_group > 1 && groups == 1) {
        return 0;
    }
    if (per_group == 1 && groups > 1) {
        return 1;
    }
    if (per_group > 1 && groups > 1) {
        return 2;
    }
    return 3;
}

static const char *mode_name(int mode) {
    static const char *names[] = {"位扩展", "字扩展", "字位同时扩展", "不用扩展"};
    if (mode < 0 || mode > 3) {
        return "未知";
    }
    return names[mode];
}

int expand_info(uint64_t target_units, int target_bits, uint64_t chip_units, int chip_bits,
                ExpandInfo *out) {
    int chip_addr;
    if (out == NULL || target_units == 0 || chip_units == 0 || target_bits <= 0 || chip_bits <= 0) {
        return 1;
    }
    if (target_bits % chip_bits != 0) {
        return 2;
    }
    if (target_units % chip_units != 0) {
        return 3;
    }
    chip_addr = exact_log2_u64(chip_units);
    if (chip_addr < 0) {
        return 4;
    }
    out->per_group = target_bits / chip_bits;
    out->groups = target_units / chip_units;
    out->chips = out->groups * (uint64_t)out->per_group;
    out->chip_addr_bits = chip_addr;
    out->select_bits = out->groups <= 1 ? 0 : ceil_log2_u64(out->groups);
    out->total_addr_bits = ceil_log2_u64(target_units);
    out->mode = expand_mode(out->groups, out->per_group);
    return out->select_bits < 0 || out->total_addr_bits < 0 ? 1 : 0;
}

int locate_info(uint64_t target_units, uint64_t chip_units, uint64_t addr, LocateInfo *out) {
    uint64_t groups;
    int chip_bits = exact_log2_u64(chip_units);
    if (out == NULL || chip_bits < 0 || target_units == 0 || target_units % chip_units != 0) {
        return 1;
    }
    if (addr >= target_units) {
        return 2;
    }
    groups = target_units / chip_units;
    out->chip_bits = chip_bits;
    out->group = addr / chip_units;
    out->min_addr = out->group * chip_units;
    out->max_addr = out->min_addr + chip_units - 1;
    out->in_chip = addr & ((1ull << chip_bits) - 1);
    out->select = addr >> chip_bits;
    out->select_bits = groups <= 1 ? 0 : ceil_log2_u64(groups);
    return 0;
}

int address_bits(uint64_t bytes) {
    return ceil_log2_u64(bytes);
}

int zone_rom(int addr_bits, int low_parts, int high_parts, uint64_t *ram_last, uint64_t *rom_first,
             uint64_t *rom_last) {
    uint64_t space;
    uint64_t parts;
    uint64_t piece;
    if (addr_bits < 1 || addr_bits > 62 || low_parts < 1 || high_parts < 1) {
        return 1;
    }
    space = 1ull << addr_bits;
    parts = (uint64_t)low_parts + (uint64_t)high_parts;
    if (space % parts != 0) {
        return 2;
    }
    piece = space / parts;
    *ram_last = piece * (uint64_t)low_parts - 1;
    *rom_first = *ram_last + 1;
    *rom_last = space - 1;
    return 0;
}

static int pattern_bit(const char *pattern, int index, int image, int *x_seen, int x_count) {
    char ch = pattern[index];
    if (ch == '0' || ch == '1') {
        return ch == '1';
    }
    *x_seen += 1;
    return (image >> (x_count - *x_seen)) & 1;
}

int cs_ranges(int addr_bits, int chip_bits, const char *pattern, AddrRange *ranges, int cap,
              int *count) {
    int high;
    int x_count = 0;
    int image;
    int i;
    if (pattern == NULL || ranges == NULL || count == NULL || addr_bits < 1 || addr_bits > 62 ||
        chip_bits < 1 || chip_bits > addr_bits) {
        return 1;
    }
    high = addr_bits - chip_bits;
    if ((int)strlen(pattern) != high) {
        return 2;
    }
    for (i = 0; i < high; i++) {
        char ch = pattern[i];
        if (ch == 'x' || ch == 'X') {
            x_count++;
        } else if (ch != '0' && ch != '1') {
            return 3;
        }
    }
    if (x_count > 4 || (1 << x_count) > cap) {
        return 4;
    }
    *count = 1 << x_count;
    for (image = 0; image < *count; image++) {
        uint64_t high_val = 0;
        int seen = 0;
        for (i = 0; i < high; i++) {
            high_val = (high_val << 1) | (uint64_t)pattern_bit(pattern, i, image, &seen, x_count);
        }
        ranges[image].start = high_val << chip_bits;
        ranges[image].end = ranges[image].start + (1ull << chip_bits) - 1;
    }
    return 0;
}

static int reject_expand(int rc) {
    if (rc == 2) {
        fprintf(stderr, "目标位宽要能被芯片位宽整除\n");
    } else if (rc == 3) {
        fprintf(stderr, "目标单元数要能被芯片单元数整除\n");
    } else if (rc == 4) {
        fprintf(stderr, "芯片单元数要是 2 的幂，片内地址才是整数位\n");
    } else if (rc != 0) {
        fprintf(stderr, "expand 的容量或位宽不合法\n");
    }
    return rc == 0 ? 0 : 1;
}

int show_expand(FILE *out, uint64_t target_units, int target_bits, uint64_t chip_units,
                int chip_bits) {
    ExpandInfo info;
    char target[96];
    char chip[96];
    if (reject_expand(expand_info(target_units, target_bits, chip_units, chip_bits, &info)) != 0) {
        return 1;
    }
    fmt_bin_count(target, sizeof target, target_units);
    fmt_bin_count(chip, sizeof chip, chip_units);
    fprintf(out, "【存储器扩展】\n");
    fprintf(out, "目标 %s 个单元 × %d 位，芯片 %s 个单元 × %d 位\n", target, target_bits, chip,
            chip_bits);
    fprintf(out, "位扩展把数据位拼起来：每组 %d 片，%d × %d = %d 位\n", info.per_group, info.per_group,
            chip_bits, target_bits);
    fprintf(out, "字扩展增加单元数：%llu 组，组与组靠片选区分，组内各片地址线并联\n",
            (unsigned long long)info.groups);
    fprintf(out, "芯片数 = %llu × %d = %llu\n", (unsigned long long)info.groups, info.per_group,
            (unsigned long long)info.chips);
    fprintf(out, "片内地址 %d 位，片选 %d 位，CPU 侧地址至少 %d 位\n", info.chip_addr_bits,
            info.select_bits, info.total_addr_bits);
    if (info.select_bits > 0 && info.select_bits < 63) {
        uint64_t outputs = 1ull << info.select_bits;
        if (outputs > info.groups) {
            fprintf(out, "组数不是 2 的幂，%d 位片选有空输出 %llu 个\n", info.select_bits,
                    (unsigned long long)(outputs - info.groups));
        }
    }
    fprintf(out, "结果：芯片数 = %llu，%s，%llu 组 × 每组 %d 片\n", (unsigned long long)info.chips,
            mode_name(info.mode), (unsigned long long)info.groups, info.per_group);
    return 0;
}

int show_locate(FILE *out, uint64_t target_units, uint64_t chip_units, uint64_t addr) {
    LocateInfo info;
    char target[96], chip[96], hex[32], lo[32], hi[32], bits[40];
    int rc = locate_info(target_units, chip_units, addr, &info);
    if (rc == 2) {
        fprintf(stderr, "地址超出了这块存储器\n");
        return 1;
    }
    if (rc != 0) {
        fprintf(stderr, "locate 要求芯片单元数是 2 的幂，并且能整除目标单元数\n");
        return 1;
    }
    fmt_bin_count(target, sizeof target, target_units);
    fmt_bin_count(chip, sizeof chip, chip_units);
    fmt_hex(hex, sizeof hex, addr);
    fmt_hex(lo, sizeof lo, info.min_addr);
    fmt_hex(hi, sizeof hi, info.max_addr);
    fprintf(out, "【片选地址】\n");
    fprintf(out, "存储器 %s 个单元，一组 %s 个单元。位扩展的几片共用这一段地址\n", target, chip);
    fprintf(out, "地址 %s 落在第 %llu 组（从 0 计）\n", hex, (unsigned long long)info.group);
    fprintf(out, "低 %d 位是片内地址，更高的 %d 位送给片选译码\n", info.chip_bits, info.select_bits);
    if (info.select_bits > 0) {
        fmt_bits(bits, sizeof bits, info.select, info.select_bits);
        fprintf(out, "片选值 = %sB = %llu\n", bits, (unsigned long long)info.select);
    }
    fprintf(out, "该组地址 %s～%s\n", lo, hi);
    fprintf(out, "结果：最小地址 = %s\n", lo);
    return 0;
}

int show_mar(FILE *out, uint64_t space_bytes) {
    int bits = address_bits(space_bytes);
    char count[96];
    if (bits < 0) {
        fprintf(stderr, "mar 的地址空间不合法\n");
        return 1;
    }
    fmt_bin_count(count, sizeof count, space_bytes);
    fprintf(out, "【MAR 位数】按字节编址\n");
    fprintf(out, "地址空间 = %s\n", count);
    fprintf(out, "MAR 要能点到其中每一个字节，位数 = %d\n", bits);
    fprintf(out, "已经插上的容量不改变 MAR。它做在 CPU 里，宽度按地址空间设计\n");
    fprintf(out, "结果：MAR 至少 %d 位\n", bits);
    return 0;
}

int show_zone(FILE *out, int addr_bits, int low_parts, int high_parts) {
    uint64_t ram_last = 0, rom_first = 0, rom_last = 0;
    char ram_lo[32], ram_hi[32], rom_lo[32], rom_hi[32];
    int parts = low_parts + high_parts;
    int digits = (addr_bits + 3) / 4;
    int rc = zone_rom(addr_bits, low_parts, high_parts, &ram_last, &rom_first, &rom_last);
    if (rc == 2) {
        fprintf(stderr, "地址空间不能按这个比例均分\n");
        return 1;
    }
    if (rc != 0) {
        fprintf(stderr, "zone 的地址位数或比例不合法\n");
        return 1;
    }
    fmt_hex_pad(ram_lo, sizeof ram_lo, 0, digits);
    fmt_hex_pad(ram_hi, sizeof ram_hi, ram_last, digits);
    fmt_hex_pad(rom_lo, sizeof rom_lo, rom_first, digits);
    fmt_hex_pad(rom_hi, sizeof rom_hi, rom_last, digits);
    fprintf(out, "【主存分区】%d 根地址线，按字节编址\n", addr_bits);
    fprintf(out, "地址空间 = 2^%d B，地址 %s～%s，芯片要把这个空间占满\n", addr_bits, ram_lo, rom_hi);
    fprintf(out, "RAM:ROM = %d:%d，RAM 放连续低地址，ROM 放连续高地址\n", low_parts, high_parts);
    fprintf(out, "RAM 地址 %s～%s\n", ram_lo, ram_hi);
    fprintf(out, "ROM 地址 %s～%s\n", rom_lo, rom_hi);
    if (exact_log2_u64((uint64_t)parts) >= 0) {
        fprintf(out, "比例的和是 2 的幂，高 %d 位可以划分这 %d 份。ROM 占最高一份\n",
                exact_log2_u64((uint64_t)parts), parts);
    }
    fprintf(out, "结果：ROM 地址范围 = %s～%s\n", rom_lo, rom_hi);
    return 0;
}

int show_cs(FILE *out, int addr_bits, int chip_bits, const char *pattern) {
    AddrRange ranges[16];
    int count = 0;
    int i;
    int rc = cs_ranges(addr_bits, chip_bits, pattern, ranges, 16, &count);
    char lo[32], hi[32];
    if (rc == 2) {
        fprintf(stderr, "片选约束的位数要等于地址线减去片内地址\n");
        return 1;
    }
    if (rc != 0) {
        fprintf(stderr, "cs 的地址位数或片选约束不合法（无关位最多 4 个，用 x 表示）\n");
        return 1;
    }
    fprintf(out, "【译码片选】地址线 %d 根，片内地址 %d 位\n", addr_bits, chip_bits);
    fprintf(out, "片内地址接 A0～A%d\n", chip_bits - 1);
    if (addr_bits > chip_bits) {
        fprintf(out, "片选看 A%d～A%d = %s（左边是高位，x 表示这根线没参加译码）\n", addr_bits - 1,
                chip_bits, pattern);
    }
    if (count == 1) {
        fprintf(out, "高位都参加了片选，是全译码，这一片只有一段地址\n");
    } else {
        fprintf(out, "有没参加译码的地址线，是部分译码，同一片会在 %d 段地址里重复出现\n", count);
    }
    for (i = 0; i < count; i++) {
        fmt_hex(lo, sizeof lo, ranges[i].start);
        fmt_hex(hi, sizeof hi, ranges[i].end);
        fprintf(out, "第 %d 段：%s～%s\n", i + 1, lo, hi);
    }
    fmt_hex(lo, sizeof lo, ranges[0].start);
    fmt_hex(hi, sizeof hi, ranges[0].end);
    fprintf(out, "结果：%s，第一段 %s～%s，共 %d 段\n", count == 1 ? "全译码" : "部分译码", lo, hi,
            count);
    return 0;
}
