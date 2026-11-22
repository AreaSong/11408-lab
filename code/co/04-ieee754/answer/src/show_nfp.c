/*
 * 把补码浮点的五步和规格化判断印出来。位型本身在 nfp.c。
 * 舍入按 2009-13 的做法截断：移出位是 0 时和任何舍入规则结果相同。
 */
#include "show.h"

#include "nfp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fmt_raw(char *buf, int width, int value) {
    int i;
    int bits = nfp_low(value, width);
    for (i = 0; i < width; i++) {
        buf[i] = ((bits >> (width - 1 - i)) & 1) ? '1' : '0';
    }
    buf[width] = '\0';
}

static void fmt_comma(char *buf, size_t n, int width, int value) {
    char raw[20];
    fmt_raw(raw, width, value);
    snprintf(buf, n, "%c%c,%s（%s）", raw[0], raw[1], raw + 2, raw);
}

static int igcd(int a, int b) {
    if (a < 0) {
        a = -a;
    }
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

static void fmt_frac(char *buf, size_t n, int numer, int mag_bits) {
    int den = 1 << mag_bits;
    int g;
    if (numer == 0) {
        snprintf(buf, n, "0");
        return;
    }
    g = igcd(numer, den);
    numer /= g;
    den /= g;
    if (den == 1) {
        snprintf(buf, n, "%d", numer);
    } else {
        snprintf(buf, n, "%d/%d", numer, den);
    }
}

static void fmt_machine(char *buf, size_t n, int exp_bits, int exp, int frac_bits, int frac) {
    char e[20];
    char f[20];
    fmt_raw(e, exp_bits, exp);
    fmt_raw(f, frac_bits, frac);
    snprintf(buf, n, "%s %s", e, f);
}

static void print_num(FILE *out, const char *name, const NfpNum *num) {
    char e[40];
    char f[48];
    char val[32];
    fmt_comma(e, sizeof e, num->exp_bits, num->exp);
    fmt_comma(f, sizeof f, num->frac_bits, num->frac);
    fmt_frac(val, sizeof val, num->frac, num->frac_bits - 2);
    if (val[0] == '-') {
        fprintf(out, "%s = 2^%d×(%s)，阶码 %s，尾数 %s\n", name, num->exp, val, e, f);
    } else {
        fprintf(out, "%s = 2^%d×%s，阶码 %s，尾数 %s\n", name, num->exp, val, e, f);
    }
}

static void print_lost(FILE *out, int n, unsigned bits, int any) {
    char text[16];
    int i;
    if (n <= 0) {
        return;
    }
    if (n > 12) {
        fprintf(out, "移出超过 12 位，%s\n", any ? "其中有 1" : "全是 0");
        return;
    }
    for (i = 0; i < n; i++) {
        text[i] = ((bits >> (n - 1 - i)) & 1) ? '1' : '0';
    }
    text[n] = '\0';
    fprintf(out, "移出的位是 %s，%s\n", text, any ? "里面有 1" : "全是 0");
}

static void print_align(FILE *out, const NfpAdd *a) {
    char e[40];
    char f[48];
    const char *who = a->y_small ? "Y" : "X";
    if (a->diff == 0) {
        fprintf(out, "对阶：阶码相同，尾数不用移动\n");
        return;
    }
    fprintf(out, "对阶：阶差 %d。小阶向大阶看齐，%s 的尾数算术右移 %d 位\n", a->diff, who, a->diff);
    print_lost(out, a->align_show, a->align_lost, a->align_any);
    fmt_comma(e, sizeof e, a->x.exp_bits, a->sum_exp);
    fmt_comma(f, sizeof f, a->x.frac_bits, a->aligned_frac);
    fprintf(out, "%s 变成阶码 %s，尾数 %s\n", who, e, f);
}

static void print_sum(FILE *out, const NfpAdd *a) {
    char left[48];
    char right[48];
    char sum[48];
    int kept = a->y_small ? a->x.frac : a->y.frac;
    int bits = nfp_low(a->sum_frac, a->x.frac_bits);
    int s2 = (bits >> (a->x.frac_bits - 2)) & 3;
    fmt_comma(left, sizeof left, a->x.frac_bits, kept);
    fmt_comma(right, sizeof right, a->x.frac_bits, a->aligned_frac);
    fmt_comma(sum, sizeof sum, a->x.frac_bits, a->sum_frac);
    fprintf(out, "尾数相加：%s + %s = %s\n", left, right, sum);
    if (a->norm_dir == 1) {
        fprintf(out, "规格化：和的双符号位是 %d%d，右规 %d 位，阶码加 %d\n", s2 >> 1, s2 & 1, a->norm_n, a->norm_n);
    } else if (a->norm_dir == -1) {
        fprintf(out, "规格化：和的双符号位是 %d%d，左规 %d 位，阶码减 %d\n", s2 >> 1, s2 & 1, a->norm_n, a->norm_n);
    } else if (a->sum_frac == 0) {
        fprintf(out, "规格化：和是 0，不做移位\n");
    } else {
        fprintf(out, "规格化：和的双符号位是 %d%d，已经规格化\n", s2 >> 1, s2 & 1);
    }
}

static void print_round(FILE *out, const NfpAdd *a) {
    if (a->align_any || a->norm_lost) {
        fprintf(out, "舍入：移出位里有 1。这套补码按截断丢掉，不做 IEEE 754 的向偶数舍入\n");
        return;
    }
    if (a->diff == 0 && a->norm_dir != 1) {
        fprintf(out, "舍入：没有移出的位\n");
        return;
    }
    fprintf(out, "舍入：移出的位都是 0，尾数不变\n");
}

static void print_value(FILE *out, const char *machine, int exp, const char *val) {
    if (val[0] == '-') {
        fprintf(out, "结果：%s，真值 2^%d×(%s)\n", machine, exp, val);
        return;
    }
    fprintf(out, "结果：%s，真值 2^%d×%s\n", machine, exp, val);
}

static void print_judge(FILE *out, const NfpAdd *a) {
    char before[40];
    char after[40];
    char exp_text[40];
    char val[32];
    int emin = -(1 << (a->x.exp_bits - 2));
    int emax = (1 << (a->x.exp_bits - 2)) - 1;
    fmt_machine(before, sizeof before, a->x.exp_bits, a->sum_exp, a->x.frac_bits, a->sum_frac);
    fmt_machine(after, sizeof after, a->x.exp_bits, a->result_exp, a->x.frac_bits, a->result_frac);
    fmt_comma(exp_text, sizeof exp_text, a->x.exp_bits, a->result_exp);
    if (a->result_frac == 0) {
        fprintf(out, "判溢出：尾数为 0\n");
        fprintf(out, "结果：0\n");
        return;
    }
    if (a->overflow != 0) {
        fprintf(out, "判溢出：阶码 %d 超出 %d 到 %d，%s。阶码写成 %s\n", a->result_exp, emin, emax,
                a->overflow > 0 ? "上溢" : "下溢", exp_text);
        fprintf(out, "规格化前 %s，规格化后 %s。阶码已经溢出，最终结果不是一个可表示的数\n", before, after);
        fprintf(out, "结果：发生溢出（阶码%s）\n", a->overflow > 0 ? "上溢" : "下溢");
        return;
    }
    fmt_frac(val, sizeof val, a->result_frac, a->x.frac_bits - 2);
    fprintf(out, "判溢出：阶码 %d 在 %d 到 %d 里\n", a->result_exp, emin, emax);
    print_value(out, after, a->result_exp, val);
}

static int show_nfp_add(FILE *out, const NfpNum *x, const NfpNum *y) {
    NfpAdd a;
    int emin = -(1 << (x->exp_bits - 2));
    int emax = (1 << (x->exp_bits - 2)) - 1;
    if (nfp_add(&a, x, y) != 0) {
        fprintf(stderr, "这两个补码浮点数加不了\n");
        return 1;
    }
    fprintf(out, "【补码浮点加法】\n");
    fprintf(out, "阶码、尾数都用补码，各含 2 位符号位。不是 IEEE 754：阶码不是移码，尾数没有隐含 1\n");
    fprintf(out, "阶码 %d 位，能表示 %d 到 %d。尾数 %d 位，小数点在双符号位后面\n", x->exp_bits, emin, emax,
            x->frac_bits);
    print_num(out, "X", x);
    print_num(out, "Y", y);
    print_align(out, &a);
    print_sum(out, &a);
    print_round(out, &a);
    print_judge(out, &a);
    return 0;
}

static void print_reason(FILE *out, int frac, int width) {
    int bits = nfp_low(frac, width);
    int s2 = (bits >> (width - 2)) & 3;
    int mag = (bits >> (width - 3)) & 1;
    fprintf(out, "双符号位 %d%d，最高数值位是 %d", s2 >> 1, s2 & 1, mag);
    if (s2 == 0 && mag == 1) {
        fprintf(out, "，形如 00.1…，正数已经规格化\n");
    } else if (s2 == 3 && mag == 0) {
        fprintf(out, "，形如 11.0…，负数已经规格化\n");
    } else if (s2 == 1) {
        fprintf(out, "，形如 01.…，尾数上溢，要右规\n");
    } else if (s2 == 2) {
        fprintf(out, "，形如 10.…，负数尾数溢出，要右规\n");
    } else if (s2 == 0) {
        fprintf(out, "，形如 00.0…，要左规\n");
    } else {
        fprintf(out, "，形如 11.1…，要左规\n");
    }
}

static int show_norm(FILE *out, const NfpNum *num) {
    char e[40];
    char f[48];
    char val[32];
    int shape = nfp_shape(num->frac, num->frac_bits);
    const char *word = "已规格化";
    fmt_comma(e, sizeof e, num->exp_bits, num->exp);
    fmt_comma(f, sizeof f, num->frac_bits, num->frac);
    fmt_frac(val, sizeof val, num->frac, num->frac_bits - 2);
    if (shape == NFP_RIGHT) {
        word = "需要右规";
    } else if (shape == NFP_LEFT) {
        word = "需要左规";
    } else if (shape == NFP_ZERO) {
        word = "尾数为 0";
    }
    fprintf(out, "【补码规格化判断】\n");
    fprintf(out, "阶码和尾数都用补码，各含 2 位符号位。规格化看尾数的双符号位，不看 IEEE 754 的隐含 1\n");
    fprintf(out, "阶码 %s = %d", e, num->exp);
    if (num->overflow > 0) {
        fprintf(out, "，这个阶码本身已经上溢\n");
    } else if (num->overflow < 0) {
        fprintf(out, "，这个阶码本身已经下溢\n");
    } else {
        fprintf(out, "\n");
    }
    fprintf(out, "尾数 %s = %s\n", f, val);
    if (shape == NFP_ZERO) {
        fprintf(out, "尾数全 0，表示 0，不做规格化\n");
    } else {
        print_reason(out, num->frac, num->frac_bits);
    }
    fprintf(out, "结果：%s\n", word);
    return 0;
}

static int fail_code(int rc) {
    if (rc == 2) {
        fprintf(stderr, "阶码超出双符号位补码能表示的范围\n");
    } else if (rc == 3) {
        fprintf(stderr, "尾数不是这套补码小数能精确表示的值\n");
    } else if (rc == 4) {
        fprintf(stderr, "尾数超出补码能存放的范围\n");
    } else {
        fprintf(stderr, "nfp 的阶码、尾数位数不合法\n");
    }
    return 1;
}

static int parse_i(const char *text, int *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < -1000000L || value > 1000000L) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int parse_frac(const char *text, int *numer, int *denom) {
    int n = 0;
    int a = 0;
    int b = 0;
    if (sscanf(text, "%d/%d%n", &a, &b, &n) == 2 && text[n] == '\0' && b != 0) {
        *numer = a;
        *denom = b;
        return 1;
    }
    if (sscanf(text, "%d%n", &a, &n) == 1 && text[n] == '\0') {
        *numer = a;
        *denom = 1;
        return 1;
    }
    return 0;
}

static int cmd_add(FILE *out, char **av, int n) {
    int exp_bits = 0;
    int frac_bits = 0;
    int e1 = 0;
    int e2 = 0;
    int p1 = 0;
    int q1 = 0;
    int p2 = 0;
    int q2 = 0;
    int rc;
    NfpNum x;
    NfpNum y;
    if (n != 8 || !parse_i(av[2], &exp_bits) || !parse_i(av[3], &frac_bits) || !parse_i(av[4], &e1) ||
        !parse_frac(av[5], &p1, &q1) || !parse_i(av[6], &e2) || !parse_frac(av[7], &p2, &q2)) {
        fprintf(stderr, "nfp add 需要：阶码位数 尾数位数 阶1 分子/分母 阶2 分子/分母\n");
        return 1;
    }
    rc = nfp_from_fraction(&x, exp_bits, frac_bits, e1, p1, q1);
    if (rc != 0) {
        return fail_code(rc);
    }
    rc = nfp_from_fraction(&y, exp_bits, frac_bits, e2, p2, q2);
    if (rc != 0) {
        return fail_code(rc);
    }
    return show_nfp_add(out, &x, &y);
}

static int cmd_norm(FILE *out, char **av, int n) {
    int exp_bits = 0;
    int frac_bits = 0;
    NfpNum num;
    if (n != 6 || !parse_i(av[2], &exp_bits) || !parse_i(av[3], &frac_bits)) {
        fprintf(stderr, "nfp norm 需要：阶码位数 尾数位数 阶码位串 尾数位串\n");
        return 1;
    }
    if (nfp_from_bits(&num, exp_bits, frac_bits, av[4], av[5]) != 0) {
        fprintf(stderr, "阶码、尾数位串的长度要正好等于位数，并且只能是 0 和 1\n");
        return 1;
    }
    return show_norm(out, &num);
}

int show_nfp_cmd(FILE *out, char **av, int n) {
    if (n >= 2 && strcmp(av[1], "add") == 0) {
        return cmd_add(out, av, n);
    }
    if (n >= 2 && strcmp(av[1], "norm") == 0) {
        return cmd_norm(out, av, n);
    }
    fprintf(stderr, "nfp 后面是 add 或 norm\n");
    return 1;
}
