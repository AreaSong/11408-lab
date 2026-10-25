#include "exam.h"

#include "beat.h"

#include <stdio.h>

int bits_needed(int kinds) {
    int bits = 0;
    int cap = 1;
    if (kinds < 1) {
        return -1;
    }
    while (cap < kinds) {
        cap *= 2;
        bits++;
    }
    return bits;
}

int addr_bits(uint64_t n) {
    int bits = 0;
    uint64_t cap = 1;
    if (n == 0) {
        return 0;
    }
    while (cap < n) {
        if (bits == 63) {
            return 64;
        }
        cap <<= 1;
        bits++;
    }
    return bits;
}

uint16_t encode15(int op, int rd, int m1, int rs1, int m2, int rs2) {
    /* 7 位操作码，1 位保留，Rd，源 1 的寻址和编号，源 2 的寻址和编号。 */
    return (uint16_t)((op << 9) | (rd << 6) | (m1 << 5) | (rs1 << 3) | (m2 << 2) | rs2);
}

static const Micro SUB15[] = {
    {"执行", "(R2)->MAR", "间接地址", "R2out MARin"},
    {"执行", "M(MAR)->MDR (R3)->T", "读主存的同时送减数", "MemR MDRinE R3out Tin"},
    {"执行", "(MDR)-(T)->SR", "主存数减 R3", "MDRout sub SRin"},
    {"写回", "(SR)->R1", "经三态门写回", "SRout R1in"},
};

static const Micro INC15[] = {
    {"执行", "(R1)+1->SR", "单操作数不必先占用 T", "R1out inc SRin"},
    {"写回", "(SR)->R1", "经三态门写回", "SRout R1in"},
};

int micro_len_sub(void) {
    return (int)(sizeof SUB15 / sizeof SUB15[0]);
}

int micro_len_inc(void) {
    return (int)(sizeof INC15 / sizeof INC15[0]);
}

static void print_code(FILE *out, const char *text, int op, int rd, int m1, int rs1, int m2, int rs2) {
    uint16_t word = encode15(op, rd, m1, rs1, m2, rs2);
    fprintf(out, "%s\n", text);
    fprintf(out, "OP=%d 保留=0 Rd=%d M1=%d Rs1=%d M2=%d Rs2=%d 机器码 %04XH\n", op, rd, m1, rs1, m2, rs2,
            word);
}

int show_2015_path(FILE *out) {
    int alu = bits_needed(7);
    int sr = bits_needed(3);
    int step = 16 / 8;
    fprintf(out, "【2015-43】单总线数据通路\n");
    fprintf(out, "程序员可见：R0 R1 R2 R3 PC\n");
    fprintf(out, "程序员不可见：T SR MAR MDR IR\n");
    fprintf(out, "暂存器 T：单总线一次只能送一个数。没有 T 时 ALU 的 A、B 会收到同一个数\n");
    fprintf(out, "ALU 7 种操作，2 位只有 4 种编码，ALUop 至少 %d 位\n", alu);
    fprintf(out, "移位 3 种操作，SRop 至少 %d 位\n", sr);
    fprintf(out, "SRout 控制三态门，接通或断开 SR 与内总线\n");
    fprintf(out, "控制器输出端：1 2 3 5 8\n");
    fprintf(out, "补线：6->9，7->4\n");
    fprintf(out, "指令 16 bit，按字节编址，占 %d B，顺序执行时 (PC)+%d\n", step, step);
    fprintf(out, "结果：可见 R0 R1 R2 R3 PC；ALUop %d 位；SRop %d 位；增量 %d B\n", alu, sr, step);
    return 0;
}

static void print_labels(FILE *out) {
    static const char *value[] = {"0", "mov", "mova", "left", "read", "sub", "mov", "SRout"};
    static const char *why[] = {
        "控制端取 0", "SR 直送", "ALU 直送 A", "SR 左移",
        "主存读", "ALU 减法", "SR 直送", "SR 的三态门",
    };
    int i;
    fprintf(out, "图 a 标号按题图顺序，后面是该信号在题干里的含义\n");
    for (i = 0; i < 8; i++) {
        fprintf(out, "%d=%s（%s）\n", i + 1, value[i], why[i]);
    }
}

int show_2015_code(FILE *out) {
    int ops = 1 << 7;
    int bad;
    uint16_t inc = encode15(1, 1, 0, 0, 0, 0);
    uint16_t shl = encode15(2, 2, 0, 1, 0, 0);
    uint16_t sub = encode15(3, 3, 1, 1, 0, 2);
    fprintf(out, "【2015-44】指令字与执行节拍\n");
    fprintf(out, "操作码 7 位，最多 %d 条指令\n", ops);
    print_code(out, "inc R1：(R1)+1->R1", 1, 1, 0, 0, 0, 0);
    print_code(out, "shl R2, R1：(R1) 左移 1 位 ->R2", 2, 2, 0, 1, 0, 0);
    print_code(out, "sub R3, (R1), R2：((R1))-(R2)->R3", 3, 3, 1, 1, 0, 2);
    print_labels(out);
    fprintf(out, "sub R1, R3, (R2) 的执行阶段\n");
    bad = micro_print(out, SUB15, micro_len_sub(), 1);
    fprintf(out, "inc R1 的执行阶段\n");
    bad += micro_print(out, INC15, micro_len_inc(), 1);
    fprintf(out, "结果：%d 条；%04XH %04XH %04XH；sub %d 个时钟周期；inc %d 个时钟周期\n", ops, inc, shl,
            sub, micro_len_sub(), micro_len_inc());
    return bad ? 1 : 0;
}
