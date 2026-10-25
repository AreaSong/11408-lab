#include "show.h"
#include "show_util.h"

static void round_line(FILE *out, const EncShow *show) {
    if (!show->inexact) {
        fprintf(out, "舍入：23 位尾数放得下，是精确值\n");
        return;
    }
    if (show->guard && !show->sticky && !show->lsb) {
        fprintf(out, "舍入：正好一半，留下的最低位已是偶数，向偶数舍入不进位\n");
        fprintf(out, "若用 0舍1入，丢掉的最高位是 1 就会进位，两种规则在这里不同\n");
        return;
    }
    if (show->guard && !show->sticky && show->lsb) {
        fprintf(out, "舍入：正好一半，留下的最低位是奇数，向偶数舍入进 1\n");
        return;
    }
    if (show->rounded_up) {
        fprintf(out, "舍入：丢掉的部分超过一半，向偶数舍入进 1\n");
        return;
    }
    fprintf(out, "舍入：丢掉的部分不到一半，向偶数舍入不进位\n");
}

static void final_fields(FILE *out, const View *view) {
    char exp_bits[16];
    char frac_bits[32];
    write_bits((uint64_t)view->exp_field, 8, exp_bits);
    write_bits(view->frac, 23, frac_bits);
    fprintf(out, "符号位 %d，尾数 %s\n", view->sign, frac_bits);
    if (view->kind == KIND_NORM) {
        fprintf(out, "规格化数。阶码 = %d + 127 = %d = %s\n", view->exp_true, view->exp_field, exp_bits);
        fprintf(out, "尾数最高位的 1 被隐含，不占上面这 23 位\n");
    } else if (view->kind == KIND_SUB) {
        fprintf(out, "阶码全 0 且尾数不是 0：非规格化数，没有隐含 1，真阶固定是 -126\n");
    } else if (view->kind == KIND_INF) {
        fprintf(out, "阶码全 1 且尾数全 0：%s无穷\n", view->sign ? "负" : "正");
    } else if (view->kind == KIND_ZERO) {
        fprintf(out, "阶码和尾数都是 0：%s0\n", view->sign ? "-" : "+");
    }
}

static void result_line(FILE *out, const EncShow *show, const View *view, const char *hex) {
    char exam[80];
    char dec[80];
    fp_exam(view, &FMT32, exam, sizeof exam);
    if (!show->inexact && fp_decimal(view, &FMT32, dec, sizeof dec)) {
        fprintf(out, "结果：%s，真值 %s\n", hex, dec);
        return;
    }
    fprintf(out, "结果：%s，真值 %s\n", hex, exam);
}

int show_enc(FILE *out, const char *text) {
    Decimal dec;
    EncShow show;
    View view;
    char err[160];
    char hex[16];
    char machine[40];
    uint32_t host;
    if (!dec_parse(text, &dec, err, sizeof err) || !fp_encode(&dec, &FMT32, &show, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    view = fp_view(show.bits, &FMT32);
    fp_hex(show.bits, 8, hex);
    fprintf(out, "【单精度编码】%s\n", text);
    fprintf(out, "符号位 = %d（%s）\n", show.sign, show.sign ? "负数" : "非负");
    if (!show.is_zero) {
        if (show.plain[0] != '\0') {
            fprintf(out, "绝对值的二进制 = %s\n", show.plain);
        } else {
            fprintf(out, "这个十进制没有有限位的二进制\n");
        }
        fprintf(out, "舍入前 = %s\n", show.sci);
        fprintf(out, "竖线左边留下，右边第一位是舍入位，再往后并成粘滞位\n");
        if (show.msb_exp < -126) {
            fprintf(out, "真阶 %d 比 -126 还小，要对齐成非规格化数再舍入\n", show.msb_exp);
        } else if (show.msb_exp > 127) {
            fprintf(out, "真阶 %d 比 127 还大，规格化数装不下\n", show.msb_exp);
        }
        round_line(out, &show);
    }
    final_fields(out, &view);
    write_bits(show.bits, 32, machine);
    fprintf(out, "机器数 = %c %.8s %s = %s\n", machine[0], machine + 1, machine + 9, hex);
    host = c_float_bits(text);
    if (host != (uint32_t)show.bits) {
        fprintf(out, "union 对照：不一致\n");
        return 1;
    }
    fprintf(out, "union 对照：和 C 的 float 位型一致\n");
    result_line(out, &show, &view, hex);
    return 0;
}
