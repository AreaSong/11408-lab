#include "orig.h"

static void print_orig_head(FILE *out, const Orig *o) {
    char xs[24];
    char ys[24];
    char ax[24];
    char ay[16];
    int n = o->x.n;
    frac_text(xs, sizeof xs, o->x);
    frac_text(ys, sizeof ys, o->y);
    frac_dot(ax, sizeof ax, o->x.mag, 2, n);
    frac_plain(ay, sizeof ay, o->y.mag, n);
    fprintf(out, "【原码一位乘】x = %s，y = %s，数值位 n = %d\n", xs, ys, n);
    fprintf(out, "积的符号 = %d ⊕ %d = %d。符号不进加法器\n", o->x.sign, o->y.sign, o->sign);
    fprintf(out, "数值位取绝对值。部分积用双符号位，初值 00.%0*d；乘数寄存器只放 |y|\n", n, 0);
    fprintf(out, "|x| = %s，|y| 的数值位是 %s\n", ax, ay);
    fprintf(out, "被乘数寄存器放 |x|，部分积寄存器初值是 0，乘数寄存器先放 |y|\n");
    fprintf(out, "步 0  初始\n");
    fprintf(out, "部分积 00.%0*d    乘数 %s\n", n, 0, ay);
}

static void print_orig_step(FILE *out, const Orig *o, int i) {
    const OrigStep *s = &o->step[i];
    char part[24];
    char part_s[24];
    char mult[16];
    char mult_s[16];
    int n = o->x.n;
    frac_dot(part, sizeof part, s->part, 2, n);
    frac_dot(part_s, sizeof part_s, s->part_s, 2, n);
    frac_plain(mult, sizeof mult, s->mult, n);
    frac_plain(mult_s, sizeof mult_s, s->mult_s, n);
    if (s->add) {
        fprintf(out, "步 %d  乘数末位是 1，部分积 + |x|\n", i + 1);
    } else {
        fprintf(out, "步 %d  乘数末位是 0，部分积 + 0\n", i + 1);
    }
    fprintf(out, "部分积 %s    乘数 %s\n", part, mult);
    if (((s->part >> (n + 1)) & 1u) == 0u && ((s->part >> n) & 1u) != 0u) {
        fprintf(out, "双符号位是 01，这是加上 |x| 的进位。右移后高位补 0，会回到 00\n");
    }
    fprintf(out, "逻辑右移，空出的高位补 0\n");
    fprintf(out, "部分积 %s    乘数 %s\n", part_s, mult_s);
}

static void print_orig_tail(FILE *out, const Orig *o) {
    char mag[24];
    char ratio[40];
    int n = o->x.n;
    long long num = o->sign ? -(long long)o->mag : (long long)o->mag;
    if (o->mag == 0) {
        num = 0;
    }
    frac_plain(mag, sizeof mag, o->mag, 2 * n);
    frac_ratio(ratio, sizeof ratio, num, 2 * n);
    fprintf(out, "规则上做 %d 次加法（加 0 也算一次）和 %d 次逻辑右移。末位为 1、真正加上 |x| 的有 %d 次\n", n, n,
            o->adds);
    fprintf(out, "乘数寄存器里现在是积的低 %d 位\n", n);
    if (o->sign && o->mag == 0) {
        fprintf(out, "加上符号后 [x·y]原 = 1.%s。原码把这个 0 记成负零，真值仍是 0\n", mag);
        fprintf(out, "结果：原码 1.%s，真值 0（负零）\n", mag);
        return;
    }
    fprintf(out, "加上符号后 [x·y]原 = %d.%s\n", o->sign, mag);
    fprintf(out, "真值 %s\n", ratio);
    fprintf(out, "结果：原码 %d.%s，真值 %s\n", o->sign, mag, ratio);
}

int orig_print(FILE *out, const Orig *o) {
    int i;
    print_orig_head(out, o);
    for (i = 0; i < o->x.n; i++) {
        print_orig_step(out, o, i);
    }
    print_orig_tail(out, o);
    return 0;
}
