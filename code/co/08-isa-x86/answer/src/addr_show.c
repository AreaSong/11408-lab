#include "addr.h"

#include "num.h"

#include <stdio.h>
#include <string.h>

static void note_fetch(FILE *out, int fetches) {
    fprintf(out, "取操作数访存 %d 次，取指另有 1 次，合计 %d 次\n", fetches, fetches + 1);
}

int show_direct(FILE *out, int word, uint64_t ninstr, uint64_t nmodes) {
    int op = 0;
    int mode = 0;
    int addr = 0;
    uint64_t hi = 0;
    uint64_t smaller = 0;
    if (direct_calc(word, ninstr, nmodes, &op, &mode, &addr, &hi) != 0) {
        return -1;
    }
    fprintf(out, "【直接寻址范围】\n");
    fprintf(out, "操作码位数和寻址方式位数都固定，先各取能装下的最短位数\n");
    if (op > 0 && num_pow2(op - 1, &smaller) == 0) {
        fprintf(out, "%llu 条指令需要 %d 位操作码，%d 位只有 %llu 种编码\n", (unsigned long long)ninstr, op, op - 1,
                (unsigned long long)smaller);
    }
    fprintf(out, "%llu 种寻址方式需要 %d 位寻址特征\n", (unsigned long long)nmodes, mode);
    fprintf(out, "地址码 = %d-%d-%d = %d 位。直接寻址的地址无符号，不能是负数\n", word, op, mode, addr);
    fprintf(out, "结果：直接寻址范围 0～%llu\n", (unsigned long long)hi);
    return 0;
}

int show_disp(FILE *out, int word, int op_bits, int nreg, int nfields) {
    int off = 0;
    int reg_bits = 0;
    int64_t lo = 0;
    int64_t hi = 0;
    if (disp_calc(word, op_bits, nreg, nfields, &off, &lo, &hi) != 0 || num_ceil_log2((uint64_t)nreg, &reg_bits) != 0) {
        return -1;
    }
    fprintf(out, "【偏移量】\n");
    fprintf(out, "通用寄存器 %d 个，编号 %d 位。指令里点名寄存器的字段有 %d 个\n", nreg, reg_bits, nfields);
    fprintf(out, "偏移量 = %d-%d-%d×%d = %d 位，按补码解释\n", word, op_bits, nfields, reg_bits, off);
    fprintf(out, "结果：偏移量 %lld～%lld\n", (long long)lo, (long long)hi);
    return 0;
}

int show_rel(FILE *out, uint64_t inst, uint64_t len, uint64_t raw, int width, int scale) {
    uint64_t pc = 0;
    int64_t disp = 0;
    uint64_t target = 0;
    if (rel_calc(inst, len, raw, width, scale, &pc, &disp, &target) != 0) {
        return -1;
    }
    fprintf(out, "【相对寻址】\n");
    fprintf(out, "本条指令地址 ");
    num_print_addr(out, inst);
    fprintf(out, "，长度 %llu 字节。取指时 PC 按指令长度增加\n", (unsigned long long)len);
    fprintf(out, "取指后的 PC = ");
    num_print_addr(out, pc);
    fprintf(out, "\n位移是 %d 位补码，位模式 0x%0*llX，真值 %lld。比例 %d：一个位移单位含 %d 个编址单位\n", width,
            (width + 3) / 4, (unsigned long long)(raw & ((width >= 32) ? 0xFFFFFFFFULL : ((1ULL << width) - 1ULL))),
            (long long)disp, scale, scale);
    fprintf(out, "目标 = 取指后的 PC + 真值×比例\n");
    if (scale != 1) {
        fprintf(out, "若公式里的 PC 还是本条指令的地址，要先加上指令长度：(PC)+%llu+OFFSET×%d\n", (unsigned long long)len,
                scale);
    }
    fprintf(out, "结果：目标地址 ");
    num_print_addr(out, target);
    fprintf(out, "\n");
    return 0;
}

static void print_bytes(FILE *out, uint64_t field, int nbytes, int little) {
    int i;
    for (i = 0; i < nbytes; i++) {
        int shift = little ? i : nbytes - 1 - i;
        if (i > 0) {
            fprintf(out, " ");
        }
        fprintf(out, "%02llX", (unsigned long long)((field >> (8 * shift)) & 0xFF));
    }
}

static int bytes_match(uint64_t field, int nbytes, const int *obs, int little) {
    int i;
    for (i = 0; i < nbytes; i++) {
        int shift = little ? i : nbytes - 1 - i;
        if (obs[i] != (int)((field >> (8 * shift)) & 0xFF)) {
            return 0;
        }
    }
    return 1;
}

int show_reloff(FILE *out, uint64_t inst, uint64_t len, uint64_t target, int bits, const int *obs, int nobs) {
    uint64_t pc = 0;
    int64_t disp = 0;
    uint64_t field = 0;
    int nbytes = bits / 8;
    int little = 0;
    int big = 0;
    if (bits % 8 != 0 || reloff_calc(inst, len, target, bits, &pc, &disp, &field) != 0) {
        return -1;
    }
    if (nobs != 0 && nobs != nbytes) {
        return -1;
    }
    fprintf(out, "【反求偏移】\n");
    fprintf(out, "call 或转移指令的长度是 %llu 字节，下一条指令的地址才是相对寻址里的 PC\n", (unsigned long long)len);
    fprintf(out, "PC = ");
    num_print_addr(out, pc);
    fprintf(out, "\n偏移量 = 目标 - PC = %lld，%d 位补码位模式 ", (long long)disp, bits);
    num_print_addr(out, field);
    fprintf(out, "\n小端字节序：");
    print_bytes(out, field, nbytes, 1);
    fprintf(out, "\n大端字节序：");
    print_bytes(out, field, nbytes, 0);
    fprintf(out, "\n");
    if (nobs > 0) {
        little = bytes_match(field, nbytes, obs, 1);
        big = bytes_match(field, nbytes, obs, 0);
        fprintf(out, "机器码里的偏移字段按低地址到高地址是给出的那一串，和%s一致\n", little ? "小端" : "大端");
        if (!little && !big) {
            return -1;
        }
    }
    fprintf(out, "结果：下一条地址 ");
    num_print_addr(out, pc);
    fprintf(out, "，偏移量 ");
    num_print_addr(out, field);
    if (nobs > 0) {
        fprintf(out, "，%s\n", little ? "小端" : "大端");
    } else {
        fprintf(out, "\n");
    }
    return 0;
}

int show_span(FILE *out, uint64_t first, uint64_t last) {
    uint64_t bytes = 0;
    if (span_calc(first, last, &bytes) != 0) {
        return -1;
    }
    fprintf(out, "【代码长度】\n");
    fprintf(out, "末字节地址减去首字节地址，再把末字节自己算进去\n");
    fprintf(out, "结果：%llu 字节（%llXH）\n", (unsigned long long)bytes, (unsigned long long)bytes);
    return 0;
}

int show_iset(FILE *out, const int *lens, int n) {
    int i;
    int low;
    int high;
    int fixed;
    if (n < 1) {
        return -1;
    }
    low = lens[0];
    high = lens[0];
    fixed = 1;
    fprintf(out, "【指令字长是否固定】\n");
    fprintf(out, "各条指令的字节数：");
    for (i = 0; i < n; i++) {
        if (lens[i] < 1) {
            return -1;
        }
        if (lens[i] != lens[0]) {
            fixed = 0;
        }
        if (lens[i] < low) {
            low = lens[i];
        }
        if (lens[i] > high) {
            high = lens[i];
        }
        if (i > 0) {
            fprintf(out, " ");
        }
        fprintf(out, "%d", lens[i]);
    }
    fprintf(out, "\n");
    if (fixed) {
        fprintf(out, "结果：定长，每条都是 %d 字节\n", low);
        return 0;
    }
    fprintf(out, "最短 %d 字节，最长 %d 字节。指令字长不固定，这是 CISC 的典型特征；RISC 的指令字长通常固定\n", low,
            high);
    fprintf(out, "结果：变长，属于 CISC 的指令字长特征\n");
    return 0;
}

int show_arr(FILE *out, uint64_t base, uint64_t ea, uint64_t scale) {
    uint64_t bytes = 0;
    uint64_t index = 0;
    if (arr_calc(base, ea, scale, &bytes, &index) != 0) {
        return -1;
    }
    fprintf(out, "【变址下标】\n");
    fprintf(out, "元素地址 ");
    num_print_addr(out, ea);
    fprintf(out, "，数组首址 ");
    num_print_addr(out, base);
    fprintf(out, "\n字节差 = %llu。变址寄存器里保存的是下标，字节偏移 = 下标×%llu\n", (unsigned long long)bytes,
            (unsigned long long)scale);
    fprintf(out, "结果：变址寄存器的内容是 %llu\n", (unsigned long long)index);
    return 0;
}

static void print_signed(FILE *out, int64_t value) {
    if (value < 0) {
        fprintf(out, "%lld", (long long)value);
        return;
    }
    num_print_addr(out, (uint64_t)value);
}

static void print_formula(FILE *out, const char *mode, const int64_t *arg, const OpResult *result) {
    if (!result->has_ea) {
        return;
    }
    if (strcmp(mode, "direct") == 0) {
        fprintf(out, "EA = A = ");
    } else if (strcmp(mode, "indirect") == 0) {
        fprintf(out, "EA = (A) = ");
    } else if (strcmp(mode, "regind") == 0) {
        fprintf(out, "EA = (R) = ");
    } else if (strcmp(mode, "stack") == 0) {
        fprintf(out, "EA = (SP) = ");
    } else if (strcmp(mode, "index") == 0) {
        fprintf(out, "EA = (IX)+A = ");
        print_signed(out, arg[0]);
        fprintf(out, "+");
        print_signed(out, arg[1]);
        fprintf(out, " = ");
    } else if (strcmp(mode, "base") == 0) {
        fprintf(out, "EA = (BR)+A = ");
        print_signed(out, arg[0]);
        fprintf(out, "+");
        print_signed(out, arg[1]);
        fprintf(out, " = ");
    } else if (strcmp(mode, "rel") == 0) {
        fprintf(out, "EA = (PC)+A = ");
        print_signed(out, arg[0]);
        fprintf(out, "+");
        print_signed(out, arg[1]);
        fprintf(out, " = ");
    } else if (strcmp(mode, "both") == 0) {
        fprintf(out, "EA = (BR)+(IX)+A = ");
        print_signed(out, arg[0]);
        fprintf(out, "+");
        print_signed(out, arg[1]);
        fprintf(out, "+");
        print_signed(out, arg[2]);
        fprintf(out, " = ");
    }
    num_print_addr(out, result->ea);
    fprintf(out, "\n");
}

static const char *mode_use(const char *mode) {
    if (strcmp(mode, "base") == 0) {
        return "基址面向操作系统：基址寄存器里是程序装入地址，形式地址是段内偏移";
    }
    if (strcmp(mode, "index") == 0) {
        return "变址面向用户：形式地址是数组首址，变址寄存器里是会变的下标";
    }
    if (strcmp(mode, "rel") == 0) {
        return "这里的 PC 已经是取指之后的值，位移直接加上去";
    }
    if (strcmp(mode, "indirect") == 0) {
        return "一次间接先按形式地址取出有效地址，再按有效地址取操作数";
    }
    if (strcmp(mode, "imm") == 0 || strcmp(mode, "reg") == 0) {
        return "操作数不在主存里，没有有效地址";
    }
    return "有效地址算出来以后，再按它访问主存才得到操作数";
}

int show_operand(FILE *out, const char *mode, const int64_t *arg, int n) {
    OpResult r;
    if (operand_eval(mode, arg, n, &r) != 0) {
        return -1;
    }
    fprintf(out, "【有效地址】\n");
    fprintf(out, "%s\n", mode_use(mode));
    print_formula(out, mode, arg, &r);
    if (r.has_ea) {
        fprintf(out, "有效地址 ");
        num_print_addr(out, r.ea);
        fprintf(out, "，操作数 ");
        num_print_addr(out, r.value);
        fprintf(out, "\n");
        note_fetch(out, r.fetches);
        fprintf(out, "结果：有效地址 ");
        num_print_addr(out, r.ea);
        fprintf(out, "，操作数 ");
        num_print_addr(out, r.value);
        fprintf(out, "\n");
        return 0;
    }
    fprintf(out, "操作数 ");
    num_print_addr(out, r.value);
    fprintf(out, "\n");
    note_fetch(out, 0);
    fprintf(out, "结果：操作数 ");
    num_print_addr(out, r.value);
    fprintf(out, "\n");
    return 0;
}
