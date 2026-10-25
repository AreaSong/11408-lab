#include "mem.h"

#include "bits.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static void print_bytes(FILE *out, unsigned long long origin, int offset, unsigned long long machine, int size,
                        bool little) {
    int i;
    for (i = 0; i < size; i++) {
        char addr[24];
        bits_write_addr(addr, sizeof addr, origin + (unsigned)offset + (unsigned)i);
        fprintf(out, "  %s：%02XH\n", addr, mem_byte(machine, size, little, i));
    }
}

static int find_hits(const Member *m, int count, unsigned long long origin, bool little, int target,
                     unsigned long long *hits, int cap) {
    int n = 0;
    int i;
    for (i = 0; i < count; i++) {
        int b;
        if (!m[i].has_value) {
            continue;
        }
        for (b = 0; b < m[i].size && n < cap; b++) {
            if (mem_byte(m[i].machine, m[i].size, little, b) == target) {
                hits[n++] = origin + (unsigned)m[i].offset + (unsigned)b;
            }
        }
    }
    return n;
}

static void print_member(FILE *out, const Member *m, unsigned long long origin, bool little) {
    char addr[24];
    char pad_addr[24];
    bits_write_addr(addr, sizeof addr, origin + (unsigned)m->offset);
    if (m->pad_before > 0) {
        bits_write_addr(pad_addr, sizeof pad_addr, origin + (unsigned)m->offset - (unsigned)m->pad_before);
        fprintf(out, "填充 %d 字节，从 %s 起（为了让 %s 按 %d 对齐）\n", m->pad_before, pad_addr, m->label,
                m->align);
    }
    fprintf(out, "%s %s：偏移 %d，地址 %s，%d 字节%s\n", m->label, m->name, m->offset, addr, m->size,
            m->has_value ? "" : "（未赋值）");
    if (m->has_value) {
        print_bytes(out, origin, m->offset, m->machine, m->size, little);
    }
}

static void result_members(FILE *out, const Member *m, int count, unsigned long long origin) {
    int i;
    for (i = 0; i < count; i++) {
        char addr[24];
        bits_write_addr(addr, sizeof addr, origin + (unsigned)m[i].offset);
        fprintf(out, "%s%s 在 %s", i == 0 ? "" : "，", m[i].name, addr);
    }
}

static void print_align_result(FILE *out, const Member *m, int count, unsigned long long origin, bool little,
                              bool find_set, int find_byte, int total) {
    unsigned long long hits[32];
    int hits_n;
    int i;
    fprintf(out, "结果：");
    if (find_set) {
        hits_n = find_hits(m, count, origin, little, find_byte, hits, 32);
        if (hits_n == 0) {
            fprintf(out, "%02XH 没有出现；", find_byte);
        } else {
            fprintf(out, "%02XH 在 ", find_byte);
            for (i = 0; i < hits_n; i++) {
                char hit[24];
                bits_write_addr(hit, sizeof hit, hits[i]);
                fprintf(out, "%s%s", i == 0 ? "" : "、", hit);
            }
            fprintf(out, "；");
        }
    } else {
        for (i = 0; i < count; i++) {
            if (m[i].has_value && m[i].offset == 0) {
                fprintf(out, "首地址内容 %02XH；", mem_byte(m[i].machine, m[i].size, little, 0));
                break;
            }
        }
    }
    result_members(out, m, count, origin);
    fprintf(out, "；大小 %d 字节\n", total);
}

static void print_align_header(FILE *out, const Member *m, int count, unsigned long long base, int max_align,
                               bool little) {
    int i;
    bool saw_double = false;
    fprintf(out, "【边界对齐 · %s】\n", little ? "小端" : "大端");
    fprintf(out, "模型：char 1/1，short 2/2，int 4/4，float 4/4，long 4/4，double 8/8（字节/对齐）\n");
    fprintf(out, "long 按卷面 32 位取 4 字节。结构体大小补到最大对齐数的倍数\n");
    for (i = 0; i < count; i++) {
        if (strcmp(m[i].type_name, "double") == 0) {
            saw_double = true;
        }
    }
    if (saw_double) {
        fprintf(out, "double 按王道对齐到 8。32 位 Linux 的 System V ABI 只对齐到 4，和王道不同\n");
    }
    if (base % (unsigned)max_align != 0) {
        fprintf(out, "首地址不是对齐数 %d 的倍数。卷面题目的结构体首地址一般已经对齐\n", max_align);
    }
}

int mem_print_align(FILE *out, unsigned long long base, bool little, int index, bool index_set, int find_byte,
                    bool find_set, Member *m, int count) {
    int max_align = 1;
    int total = 0;
    int tail;
    int i;
    unsigned long long origin;
    char base_text[24];
    char origin_text[24];
    if (count < 1 || count > MEM_MAX_MEMBER) {
        return fail("成员个数要在 1 到 16 之间");
    }
    mem_layout(m, count, &max_align, &total);
    if (index < 0 || (total > 0 && (unsigned)index > (~0ULL / (unsigned)total))) {
        return fail("数组下标不合法");
    }
    origin = base + (unsigned long long)index * (unsigned long long)total;
    bits_write_addr(base_text, sizeof base_text, base);
    bits_write_addr(origin_text, sizeof origin_text, origin);
    print_align_header(out, m, count, base, max_align, little);
    if (index_set) {
        fprintf(out, "元素大小 %d 字节。元素 [%d] 的首地址 = %s + %d × %d = %s\n", total, index, base_text, index,
                total, origin_text);
    } else {
        fprintf(out, "首地址 %s\n", origin_text);
    }
    for (i = 0; i < count; i++) {
        print_member(out, &m[i], origin, little);
    }
    tail = total - (m[count - 1].offset + m[count - 1].size);
    if (tail > 0) {
        char tail_addr[24];
        bits_write_addr(tail_addr, sizeof tail_addr, origin + (unsigned)(total - tail));
        fprintf(out, "末尾填充 %d 字节，从 %s 起（大小要是 %d 的倍数）\n", tail, tail_addr, max_align);
    }
    fprintf(out, "结构体大小 %d 字节\n", total);
    print_align_result(out, m, count, origin, little, find_set, find_byte, total);
    return 0;
}

static int load_machine(const char *pattern, int nbytes, unsigned long long *machine) {
    char err[120];
    if (nbytes < 1 || nbytes > 8) {
        return fail("字节数要在 1 到 8 之间");
    }
    if (!bits_parse_hex(pattern, machine, err, sizeof err)) {
        return fail(err);
    }
    if (nbytes < 8 && *machine > bits_mask(nbytes * 8)) {
        return fail("机器数超出给出的字节数");
    }
    return 0;
}

static void print_one_endian(FILE *out, const char *title, bool little, unsigned long long addr, int nbytes,
                             unsigned long long machine) {
    int i;
    fprintf(out, "%s：%s放在低地址\n", title, little ? "低字节" : "高字节");
    for (i = 0; i < nbytes; i++) {
        char text[24];
        bits_write_addr(text, sizeof text, addr + (unsigned)i);
        fprintf(out, "%s：%02XH\n", text, mem_byte(machine, nbytes, little, i));
    }
}

int mem_print_endian(FILE *out, const char *mode, unsigned long long addr, int nbytes, const char *pattern,
                     bool has_query, unsigned long long query) {
    unsigned long long machine = 0;
    bool do_le = strcmp(mode, "le") == 0 || strcmp(mode, "both") == 0;
    bool do_be = strcmp(mode, "be") == 0 || strcmp(mode, "both") == 0;
    char qtext[24];
    if (!do_le && !do_be) {
        return fail("端序用 le、be 或 both");
    }
    if (load_machine(pattern, nbytes, &machine) != 0) {
        return 1;
    }
    fprintf(out, "【大小端】机器数 %s，%d 字节，首地址 ", pattern, nbytes);
    bits_write_addr(qtext, sizeof qtext, addr);
    fprintf(out, "%s\n", qtext);
    if (do_le) {
        print_one_endian(out, "小端", true, addr, nbytes, machine);
    }
    if (do_be) {
        print_one_endian(out, "大端", false, addr, nbytes, machine);
    }
    if (!has_query) {
        fprintf(out, "结果：已按地址从低到高列出\n");
        return 0;
    }
    if (query < addr || query >= addr + (unsigned)nbytes) {
        return fail("要查的地址不在这个数占用的单元里");
    }
    bits_write_addr(qtext, sizeof qtext, query);
    fprintf(out, "结果：");
    if (do_le) {
        fprintf(out, "小端 %s 中是 %02XH%s", qtext, mem_byte(machine, nbytes, true, (int)(query - addr)),
                do_be ? "；" : "\n");
    }
    if (do_be) {
        fprintf(out, "大端 %s 中是 %02XH\n", qtext, mem_byte(machine, nbytes, false, (int)(query - addr)));
    }
    return 0;
}
