#include "show.h"
#include "show_util.h"

static void format_sig(uint32_t sig, char *buf, size_t n) {
    char tmp[48];
    int p = 0;
    int top = 24;
    int w;
    int last;
    if (sig == 0) {
        snprintf(buf, n, "0.0");
        return;
    }
    while (top > 23 && ((sig >> top) & 1u) == 0) {
        top--;
    }
    for (w = top; w >= 23; w--) {
        tmp[p++] = ((sig >> w) & 1u) ? '1' : '0';
    }
    tmp[p++] = '.';
    last = p;
    for (w = 22; w >= 0; w--) {
        tmp[p++] = ((sig >> w) & 1u) ? '1' : '0';
        if ((sig >> w) & 1u) {
            last = p;
        }
    }
    if (last == (top >= 23 ? (top - 23 + 1) : 1) + 1) {
        tmp[last++] = '0';
    }
    tmp[last] = '\0';
    snprintf(buf, n, "%s", tmp);
}

static void align_line(FILE *out, const AddInfo *info) {
    char small[48];
    const char *who = info->y_is_small ? "y" : "x";
    format_sig(info->aligned, small, sizeof small);
    if (info->diff == 0) {
        fprintf(out, "对阶：真阶相同，阶差 0，尾数不用移动\n");
        return;
    }
    fprintf(out, "对阶：阶差 %d。小阶向大阶看齐，%s 的尾数右移 %d 位，变成 %s\n", info->diff, who,
            info->diff, small);
    if (info->aligned == 0 && info->diff > 0) {
        if (info->norm == 0 && info->guard) {
            fprintf(out, "24 位尾数被移空，丢掉的最高位是 1，它落在舍入位上\n");
        } else {
            fprintf(out, "24 位尾数被移空，丢掉的位还要参加舍入\n");
        }
    } else if (info->align_lost && info->x.sign != info->y.sign) {
        fprintf(out, "右移丢掉的位不是 0。减法要按这些位再借 1，否则尾数会偏大\n");
    } else if (info->align_lost) {
        fprintf(out, "右移丢掉的位不是 0，记进粘滞位，留给舍入\n");
    }
}

static void norm_line(FILE *out, const AddInfo *info) {
    if (info->norm > 0) {
        fprintf(out, "规格化：和 ≥ 2，右规 %d 位，真阶加 %d\n", info->norm, info->norm);
        return;
    }
    if (info->norm < 0) {
        fprintf(out, "规格化：和 < 1，左规 %d 位，真阶减 %d\n", -info->norm, -info->norm);
        return;
    }
    if (info->z.kind == KIND_SUB) {
        fprintf(out, "规格化：尾数小于 1，但真阶已经是 -126，不能再左规\n");
        return;
    }
    fprintf(out, "规格化：尾数已经是 1.M，不用移位\n");
}

static void round_line(FILE *out, const AddInfo *info) {
    if (!info->guard && !info->sticky) {
        fprintf(out, "舍入：没有移出的 1，不进位\n");
        return;
    }
    if (info->guard && !info->sticky && !info->lsb) {
        fprintf(out, "舍入：正好一半，最低位已是偶数，向偶数舍入不进位\n");
        fprintf(out, "0舍1入 这时会进位，和默认的向偶数舍入不一样\n");
        return;
    }
    if (info->round_up) {
        fprintf(out, "舍入：向偶数舍入进 1\n");
        return;
    }
    fprintf(out, "舍入：向偶数舍入不进位\n");
}

static void over_line(FILE *out, const AddInfo *info) {
    if (info->z.kind == KIND_INF) {
        fprintf(out, "判溢出：真阶超过 127，上溢，结果是无穷\n");
    } else if (info->z.kind == KIND_ZERO && info->over < 0) {
        fprintf(out, "判溢出：结果太小，下溢成 %s0\n", info->z.sign ? "-" : "+");
    } else if (info->z.kind == KIND_SUB) {
        fprintf(out, "判溢出：没有上溢。结果是非规格化数，真阶按 -126\n");
    } else if (info->z.kind == KIND_ZERO) {
        fprintf(out, "判溢出：和是 0，没有溢出\n");
    } else {
        fprintf(out, "判溢出：真阶 %d 在 -126 到 127 之间，无溢出\n", info->z.exp_true);
    }
}

static void five_steps(FILE *out, const AddInfo *info) {
    char bigs[48];
    char smalls[48];
    char sum[48];
    uint32_t big = info->y_is_small ? info->x_sig : info->y_sig;
    uint32_t small = info->aligned;
    const char *op = (info->x.sign == info->y.sign) ? "+" : "-";
    if (info->diff == 0) {
        small = info->y_is_small ? info->y_sig : info->x_sig;
    }
    format_sig(big, bigs, sizeof bigs);
    format_sig(small, smalls, sizeof smalls);
    format_sig(info->sum_sig, sum, sizeof sum);
    align_line(out, info);
    fprintf(out, "尾数相%s：大阶 %s %s 小阶 %s = %s\n", (info->x.sign == info->y.sign) ? "加" : "减",
            bigs, op, smalls, sum);
    norm_line(out, info);
    round_line(out, info);
    over_line(out, info);
}

static int finish(FILE *out, const AddInfo *info) {
    char hex[16];
    char value[80];
    fp_hex(info->z_bits, 8, hex);
    value_text(info->z_bits, value, sizeof value);
    if (info->c_match == 0) {
        fprintf(out, "和 C 的 float 相加不一致\n");
        return 1;
    }
    if (info->c_match == 2) {
        fprintf(out, "和 C 的 float 相加都得到 NaN\n");
    } else {
        fprintf(out, "和 C 的 float 相加得到的位型一致\n");
    }
    fprintf(out, "结果：%s，真值 %s\n", hex, value);
    return 0;
}

int show_add(FILE *out, const char *a, const char *b, int is_sub) {
    uint32_t x = 0;
    uint32_t y = 0;
    AddInfo info;
    char err[160];
    char vx[80];
    char vy[80];
    char hx[16];
    char hy[16];
    if (!load_float(a, &x, err, sizeof err) || !load_float(b, &y, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    fprintf(out, "【单精度%s】%s %s %s\n", is_sub ? "减法" : "加法", a, is_sub ? "-" : "+", b);
    if (is_sub) {
        fprintf(out, "减法是加上相反数，先把 y 的符号位取反\n");
        y ^= 0x80000000u;
    }
    fadd_info(x, y, &info);
    fp_hex(x, 8, hx);
    fp_hex(y, 8, hy);
    value_text(x, vx, sizeof vx);
    value_text(y, vy, sizeof vy);
    fprintf(out, "x = %s，真值 %s\n", hx, vx);
    fprintf(out, "y = %s，真值 %s\n", hy, vy);
    if (info.special) {
        fprintf(out, "%s\n", info.note);
    } else {
        five_steps(out, &info);
    }
    return finish(out, &info);
}
