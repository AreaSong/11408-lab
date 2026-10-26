#include "micro.h"

#include "ctrl.h"
#include "wire.h"

enum { NEXT_HALT = -1, NEXT_OP = -2, MICRO_MAX = 32 };

typedef struct {
    int op;
    int beat;
    int zero;
    int next;
    int alt;
    uint32_t sig;
} UIns;

static UIns tab[MICRO_MAX];
static int ntab;
static int ready;
static int entry[10];

static void emit(int op, int beat, int zero, int next, int alt) {
    UIns *u = &tab[ntab];
    u->op = op;
    u->beat = beat;
    u->zero = zero;
    u->next = next;
    u->alt = alt;
    u->sig = wire_sig(op < 0 ? OP_NOP : op, beat, zero);
    if (op >= 0 && entry[op] < 0) {
        entry[op] = ntab;
    }
    ntab++;
}

static void micro_init(void) {
    int i;
    if (ready) {
        return;
    }
    for (i = 0; i < 10; i++) {
        entry[i] = -1;
    }
    emit(-1, 0, 0, 1, -1);
    emit(-1, 1, 0, 2, -1);
    emit(-1, 2, 0, 3, -1);
    emit(-1, 3, 0, NEXT_OP, -1); /* 下一条微地址由操作码形成 */
    emit(OP_NOP, 4, 0, 0, -1);
    emit(OP_LDA, 4, 0, 6, -1);
    emit(OP_LDA, 5, 0, 7, -1);
    emit(OP_LDA, 6, 0, 0, -1);
    emit(OP_STA, 4, 0, 9, -1);
    emit(OP_STA, 5, 0, 10, -1);
    emit(OP_STA, 6, 0, 0, -1);
    emit(OP_ADD, 4, 0, 12, -1);
    emit(OP_ADD, 5, 0, 13, -1);
    emit(OP_ADD, 6, 0, 14, -1);
    emit(OP_ADD, 7, 0, 0, -1);
    emit(OP_DIV, 4, 0, 16, -1);
    emit(OP_DIV, 5, 0, 17, 19); /* 除数为 0 时不能去做除法 */
    emit(OP_DIV, 6, 0, 18, -1);
    emit(OP_DIV, 7, 0, 0, -1);
    emit(OP_DIV, 6, 1, 0, -1);
    emit(OP_JMP, 4, 0, 0, -1);
    emit(OP_TRAP, 4, 0, 0, -1);
    emit(OP_HLT, 4, 0, NEXT_HALT, -1);
    emit(OP_IRET, 4, 0, 0, -1);
    ready = 1;
}

int micro_count(void) {
    micro_init();
    return ntab;
}

static uint32_t lookup(int op, int beat, int zero) {
    int i;
    micro_init();
    for (i = 0; i < ntab; i++) {
        if (tab[i].beat != beat) {
            continue;
        }
        if (beat < 4) {
            return tab[i].sig;
        }
        if (tab[i].op != op) {
            continue;
        }
        if (op == OP_DIV && beat == 6 && tab[i].zero != zero) {
            continue;
        }
        return tab[i].sig;
    }
    return 0xFFFFFFFFu;
}

int toy_field_control(void) {
    int rest = SIG_N - 5 - 2 - 2;
    return field_bits(5) + field_bits(2) + field_bits(2) + rest;
}

int toy_direct_word(void) {
    return SIG_N + 1 + ceil_log2(micro_count());
}

int toy_actual_bits(void) {
    return micro_count() * toy_direct_word();
}

int toy_max_bits(void) {
    return (1 << ceil_log2(micro_count())) * toy_direct_word();
}

int toy_field_word(void) {
    return toy_field_control() + 1 + ceil_log2(micro_count());
}

int toy_field_actual(void) {
    return micro_count() * toy_field_word();
}

int toy_field_max(void) {
    return (1 << ceil_log2(micro_count())) * toy_field_word();
}

static int cover_miss(void) {
    int op;
    int miss = 0;
    for (op = OP_NOP; op <= OP_IRET; op++) {
        int zero;
        int zmax = (op == OP_DIV) ? 1 : 0;
        for (zero = 0; zero <= zmax; zero++) {
            int beat;
            int n = wire_beats(op, zero);
            for (beat = 0; beat < n; beat++) {
                uint32_t got = lookup(op, beat, zero);
                uint32_t want = wire_sig(op, beat, zero);
                if (got != want || out_count(want) > 1) {
                    miss++;
                }
            }
        }
    }
    return miss;
}

int micro_mismatches(void) {
    int miss;
    micro_init();
    miss = cover_miss();
    if (ntab != 24) miss++;
    if (tab[3].next != NEXT_OP) miss++;
    if (tab[16].next != 17 || tab[16].alt != 19) miss++;
    if (tab[19].zero != 1 || tab[19].beat != 6) miss++;
    if (tab[22].next != NEXT_HALT || tab[22].op != OP_HLT) miss++;
    if (tab[23].op != OP_IRET) miss++;
    if (entry[OP_NOP] != 4 || entry[OP_LDA] != 5 || entry[OP_STA] != 8) miss++;
    if (entry[OP_ADD] != 11 || entry[OP_DIV] != 15 || entry[OP_JMP] != 20) miss++;
    if (entry[OP_TRAP] != 21 || entry[OP_HLT] != 22 || entry[OP_IRET] != 23) miss++;
    return miss;
}

static void format_next(char *buf, size_t n, int next, int alt) {
    if (next == NEXT_OP) {
        snprintf(buf, n, "由操作码形成");
    } else if (next == NEXT_HALT) {
        snprintf(buf, n, "停止");
    } else if (alt >= 0) {
        snprintf(buf, n, "%d（除数为 0 则 %d）", next, alt);
    } else {
        snprintf(buf, n, "%d", next);
    }
}

static const char *stage_name(int beat) {
    if (beat <= 2) return "取指";
    if (beat == 3) return "译码";
    return "执行";
}

static void print_table(FILE *out) {
    int i;
    micro_init();
    for (i = 0; i < ntab; i++) {
        char sig[160];
        char next[64];
        UIns *u = &tab[i];
        format_sigs(sig, sizeof sig, u->sig);
        format_next(next, sizeof next, u->next, u->alt);
        fprintf(out, "μ%d ", i);
        if (u->beat >= 4 && u->op >= 0) {
            fprintf(out, "%s ", op_name(u->op));
        }
        if (u->zero) {
            fprintf(out, "除数为 0 ");
        }
        fprintf(out, "%s T%d | %s | 下地址 %s\n", stage_name(u->beat), u->beat, sig, next);
    }
}

static void print_entries(FILE *out) {
    static const int ops[] = {
        OP_NOP, OP_LDA, OP_STA, OP_ADD, OP_DIV, OP_JMP, OP_TRAP, OP_HLT, OP_IRET
    };
    int i;
    fprintf(out, "入口");
    for (i = 0; i < 9; i++) {
        fprintf(out, " %s=%d", op_name(ops[i]), entry[ops[i]]);
    }
    fprintf(out, "\n");
}

static void print_size(FILE *out) {
    int next_bits = ceil_log2(micro_count());
    int cells = 1 << next_bits;
    int rest = SIG_N - 5 - 2 - 2;
    fprintf(out, "微命令 %d 个，直接编码操作控制字段 %d 位\n", SIG_N, SIG_N);
    fprintf(out, "判别字段 1 位。为 0 时按下地址走；为 1 且除数为 0 时改走备用下地址\n");
    fprintf(out, "微指令 %d 条，下地址字段至少 %d 位\n", micro_count(), next_bits);
    fprintf(out, "直接编码字长 %d 位 + 1 位 + %d 位 = %d 位\n", SIG_N, next_bits, toy_direct_word());
    fprintf(out, "控存实际容量 %d 条 × %d 位/条 = %d 位\n",
            micro_count(), toy_direct_word(), toy_actual_bits());
    fprintf(out, "下地址 %d 位最多寻址 %d 条\n", next_bits, cells);
    fprintf(out, "控存最大容量 %d 条 × %d 位/条 = %d 位\n", cells, toy_direct_word(), toy_max_bits());
    fprintf(out, "字段直接编码：内总线输出 5 个互斥微命令，字段 %d 位\n", field_bits(5));
    fprintf(out, "ALU 的 Add 与 Div 互斥，字段 %d 位。MemR 与 MemW 互斥，字段 %d 位\n",
            field_bits(2), field_bits(2));
    fprintf(out, "MDRoutE 走数据总线，可以和某一次内总线输出同时有效，留在直接编码里\n");
    fprintf(out, "其余 %d 个微命令直接编码 %d 位，操作控制字段 %d 位，比直接编码少 %d 位\n",
            rest, rest, toy_field_control(), SIG_N - toy_field_control());
    fprintf(out, "字段编码字长 %d 位 + 1 位 + %d 位 = %d 位\n",
            toy_field_control(), next_bits, toy_field_word());
    fprintf(out, "控存实际容量 %d 条 × %d 位/条 = %d 位\n",
            micro_count(), toy_field_word(), toy_field_actual());
    fprintf(out, "控存最大容量 %d 条 × %d 位/条 = %d 位\n",
            cells, toy_field_word(), toy_field_max());
}

int show_micro(FILE *out) {
    fprintf(out, "【微程序】与硬布线同一组控制信号\n");
    print_table(out);
    print_entries(out);
    print_size(out);
    fprintf(out, "结果：直接编码字长 %d 位，实际容量 %d 位，字段编码字长 %d 位，实际容量 %d 位\n",
            toy_direct_word(), toy_actual_bits(), toy_field_word(), toy_field_actual());
    return 0;
}
