#include "show.h"
#include "show_util.h"

static void sig_binary(const View *view, char *buf, size_t n) {
    char body[40];
    int p = 2;
    int last = 2;
    int i;
    body[0] = (view->kind == KIND_NORM) ? '1' : '0';
    body[1] = '.';
    for (i = 0; i < 23; i++) {
        int bit = (int)((view->frac >> (22 - i)) & 1ULL);
        body[p++] = bit ? '1' : '0';
        if (bit) {
            last = p;
        }
    }
    if (last == 2) {
        body[p = 2] = '0';
        last = 3;
    }
    body[last++] = 'B';
    body[last] = '\0';
    snprintf(buf, n, "%s", body);
}

static void dump_fields(FILE *out, const View *view) {
    char exp_bits[16];
    char frac_bits[32];
    char mant[40];
    write_bits((uint64_t)view->exp_field, 8, exp_bits);
    write_bits(view->frac, 23, frac_bits);
    fprintf(out, "符号 = %d，阶码 = %s = %d，尾数 = %s\n", view->sign, exp_bits, view->exp_field,
            frac_bits);
    if (view->kind == KIND_NORM || view->kind == KIND_SUB) {
        sig_binary(view, mant, sizeof mant);
    }
    if (view->kind == KIND_NORM) {
        fprintf(out, "阶码在 1 到 254 之间，是规格化数。真阶 = %d - 127 = %d\n", view->exp_field,
                view->exp_true);
        fprintf(out, "补上隐含的 1 之后，尾数是 %s\n", mant);
    } else if (view->kind == KIND_SUB) {
        fprintf(out, "阶码全 0、尾数不是 0：非规格化数。不要补 1，真阶用 -126 而不是 -127\n");
        fprintf(out, "尾数按 0.M 读，是 %s × 2^(-126)\n", mant);
    } else if (view->kind == KIND_ZERO) {
        fprintf(out, "阶码和尾数都是 0：这是 %s0，不是非规格化数\n", view->sign ? "-" : "+");
    } else if (view->kind == KIND_INF) {
        fprintf(out, "阶码全 1、尾数全 0：%s无穷\n", view->sign ? "负" : "正");
    } else {
        fprintf(out, "阶码全 1、尾数不是 0：NaN。它不是一个实数\n");
    }
}

int show_dec(FILE *out, const char *text) {
    uint64_t bits64 = 0;
    uint32_t bits;
    View view;
    char err[160];
    char hex[16];
    char exam[80];
    char dec[80];
    char machine[40];
    if (!hex_parse(text, 8, &bits64, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    bits = (uint32_t)bits64;
    view = fp_view(bits, &FMT32);
    fp_hex(bits, 8, hex);
    fp_exam(&view, &FMT32, exam, sizeof exam);
    write_bits(bits, 32, machine);
    fprintf(out, "【单精度解码】%s\n", hex);
    fprintf(out, "32 位 = %c %.8s %s\n", machine[0], machine + 1, machine + 9);
    dump_fields(out, &view);
    if (fp_decimal(&view, &FMT32, dec, sizeof dec) && strcmp(dec, exam) != 0) {
        fprintf(out, "真值 %s，十进制 %s\n", exam, dec);
    } else {
        fprintf(out, "真值 %s\n", exam);
    }
    fprintf(out, "结果：%s\n", exam);
    return 0;
}
