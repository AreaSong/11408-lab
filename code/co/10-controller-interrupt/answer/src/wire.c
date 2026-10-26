#include "wire.h"

#include "ctrl.h"

static const char *const k_expr[] = {
    "PCout = T0",
    "MARin = T0 + T4·(LDA+STA+ADD+DIV)",
    "MemR = T1 + T5·(LDA+ADD+DIV)",
    "MDRinE = T1 + T5·(LDA+ADD+DIV)",
    "PC+1 = T1",
    "MDRout = T2 + T6·LDA + T6·ADD + T6·DIV·非零",
    "IRin = T2",
    "IRout = T4·(LDA+STA+ADD+DIV+JMP)",
    "ACCin = T6·LDA + T7·(ADD+DIV·非零)",
    "ACCout = T5·STA + T7·(ADD+DIV·非零)",
    "Ain = T6·(ADD+DIV·非零)",
    "Add = T7·ADD",
    "Div = T7·DIV·非零",
    "MDRin = T5·STA",
    "MDRoutE = T6·STA",
    "MemW = T6·STA",
    "PCin = T4·(JMP+IRET)",
    "SavePC = T4·TRAP + T6·DIV·零",
    "IFclr = SavePC",
    "Vec = SavePC",
    "SPCout = T4·IRET",
    "IFset = T4·IRET",
    NULL
};

static uint32_t fetch_sig(int beat) {
    if (beat == 0) {
        return SIG_PCOUT | SIG_MARIN;
    }
    if (beat == 1) {
        return SIG_MEMR | SIG_MDRINE | SIG_PCPLUS;
    }
    if (beat == 2) {
        return SIG_MDROUT | SIG_IRIN;
    }
    return 0;
}

static uint32_t mem_sig(int op, int beat, int zero) {
    if (op == OP_LDA) {
        if (beat == 4) return SIG_IROUT | SIG_MARIN;
        if (beat == 5) return SIG_MEMR | SIG_MDRINE;
        return SIG_MDROUT | SIG_ACCIN;
    }
    if (op == OP_STA) {
        if (beat == 4) return SIG_IROUT | SIG_MARIN;
        if (beat == 5) return SIG_ACCOUT | SIG_MDRIN;
        return SIG_MDROUTE | SIG_MEMW;
    }
    if (op == OP_ADD) {
        if (beat == 4) return SIG_IROUT | SIG_MARIN;
        if (beat == 5) return SIG_MEMR | SIG_MDRINE;
        if (beat == 6) return SIG_MDROUT | SIG_AIN;
        return SIG_ACCOUT | SIG_ADD | SIG_ACCIN;
    }
    if (beat == 4) return SIG_IROUT | SIG_MARIN;
    if (beat == 5) return SIG_MEMR | SIG_MDRINE;
    if (beat == 6 && zero) return SIG_SAVEPC | SIG_IFCLR | SIG_VEC;
    if (beat == 6) return SIG_MDROUT | SIG_AIN;
    return SIG_ACCOUT | SIG_DIV | SIG_ACCIN;
}

static uint32_t exec_sig(int op, int beat, int zero) {
    if (op == OP_LDA || op == OP_STA || op == OP_ADD || op == OP_DIV) {
        return mem_sig(op, beat, zero);
    }
    if (op == OP_JMP) return SIG_IROUT | SIG_PCIN;
    if (op == OP_TRAP) return SIG_SAVEPC | SIG_IFCLR | SIG_VEC;
    if (op == OP_IRET) return SIG_SPCOUT | SIG_PCIN | SIG_IFSET;
    (void)beat;
    return 0;
}

int wire_beats(int op, int zero) {
    int exec = 1;
    if (op == OP_LDA || op == OP_STA) exec = 3;
    else if (op == OP_ADD) exec = 4;
    else if (op == OP_DIV) exec = zero ? 3 : 4;
    return 4 + exec;
}

uint32_t wire_sig(int op, int beat, int zero) {
    if (beat < 0 || beat >= wire_beats(op, zero)) {
        return 0;
    }
    if (beat <= 3) {
        return fetch_sig(beat);
    }
    return exec_sig(op, beat, zero);
}

static void print_op(FILE *out, int op, int zero, const char *title) {
    int beat;
    int n = wire_beats(op, zero);
    char sig[160];
    fprintf(out, "%s\n", title);
    for (beat = 0; beat < n; beat++) {
        uint32_t bits = wire_sig(op, beat, zero);
        const char *stage = beat <= 2 ? "取指" : (beat == 3 ? "译码" : "执行");
        format_sigs(sig, sizeof sig, bits);
        fprintf(out, "T%d %s | %s | 内总线驱动 %d 个\n", beat, stage, sig, out_count(bits));
    }
}

int show_wire(FILE *out) {
    int i;
    fprintf(out, "【硬布线】节拍电位和操作码译码相与\n");
    fprintf(out, "组合逻辑\n");
    for (i = 0; k_expr[i] != NULL; i++) {
        fprintf(out, "%s\n", k_expr[i]);
    }
    fprintf(out, "T3 译码、NOP 的 T4、HLT 的 T4 不发微命令。取指与操作码无关\n");
    fprintf(out, "同一拍里 PCout、MDRout、IRout、ACCout、SPCout 最多一个\n");
    fprintf(out, "PC+1 走增量器，Vec 走中断入口的专用通路，都不占内总线\n");
    fprintf(out, "真值表\n");
    print_op(out, OP_NOP, 0, "NOP");
    print_op(out, OP_LDA, 0, "LDA");
    print_op(out, OP_STA, 0, "STA");
    print_op(out, OP_ADD, 0, "ADD");
    print_op(out, OP_JMP, 0, "JMP");
    print_op(out, OP_TRAP, 0, "TRAP");
    print_op(out, OP_HLT, 0, "HLT");
    print_op(out, OP_IRET, 0, "IRET");
    print_op(out, OP_DIV, 0, "DIV 除数非 0");
    print_op(out, OP_DIV, 1, "DIV 除数为 0");
    fprintf(out, "结果：微命令 %d 个\n", SIG_N);
    return 0;
}
