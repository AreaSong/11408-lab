#include "booth.h"

static const char *op_text(int op) {
    if (op > 0) {
        return "+ [x]补";
    }
    if (op < 0) {
        return "+ [-x]补";
    }
    return "+ 0";
}

static void print_booth_head(FILE *out, const Booth *o) {
    char xs[24];
    char ys[24];
    char xt[24];
    char nx[24];
    char yt[24];
    int n = o->x.n;
    frac_text(xs, sizeof xs, o->x);
    frac_text(ys, sizeof ys, o->y);
    frac_dot(xt, sizeof xt, o->x_tc, 2, n);
    frac_dot(nx, sizeof nx, o->nx_tc, 2, n);
    frac_dot(yt, sizeof yt, o->y_tc, 1, n);
    fprintf(out, "【补码一位乘 · Booth】x = %s，y = %s，数值位 n = %d\n", xs, ys, n);
    fprintf(out, "符号位参加运算。被乘数和部分积用双符号位，乘数用单符号位，附加位初值 0\n");
    fprintf(out, "[x]补 = %s，[-x]补 = %s，[y]补 = %s\n", xt, nx, yt);
    fprintf(out, "末两位按「乘数当前最低位、附加位」来看：01 加 [x]补，10 加 [-x]补，00 和 11 加 0\n");
    fprintf(out, "共 n+1 = %d 次判断，只算术右移 n = %d 次。最后一次不再移位\n", n + 1, n);
    fprintf(out, "右移时部分积的最低位进入乘数寄存器最高位，那是乘积的低位\n");
    fprintf(out, "步 0  初始\n");
    fprintf(out, "部分积 00.%0*d    乘数 %s    附加位 0\n", n, 0, yt);
}

static void print_booth_regs(FILE *out, unsigned part, unsigned q, int extra, int n) {
    char pt[24];
    char qt[24];
    frac_dot(pt, sizeof pt, part, 2, n);
    frac_dot(qt, sizeof qt, q, 1, n);
    fprintf(out, "部分积 %s    乘数 %s    附加位 %d\n", pt, qt, extra);
}

static void print_booth_step(FILE *out, const Booth *o, int i) {
    const BoothStep *s = &o->step[i];
    int n = o->x.n;
    int fill = (int)((s->part >> (n + 1)) & 1u);
    fprintf(out, "步 %d  末两位 %d%d，部分积 %s\n", i + 1, (int)(s->q & 1u), s->extra, op_text(s->op));
    print_booth_regs(out, s->part, s->q, s->extra, n);
    if (!s->shifted) {
        fprintf(out, "最后一步不移位\n");
        return;
    }
    fprintf(out, "算术右移，空出的高位补 %d\n", fill);
    print_booth_regs(out, s->part_s, s->q_s, s->extra_s, n);
}

static void print_booth_tail(FILE *out, const Booth *o) {
    char bits[40];
    char ratio[40];
    int n = o->x.n;
    frac_dot(bits, sizeof bits, o->product, 1, 2 * n);
    frac_ratio(ratio, sizeof ratio, o->value, 2 * n);
    fprintf(out, "判断 %d 次，其中真正加减 %d 次，算术右移 %d 次\n", n + 1, o->real_adds, n);
    fprintf(out, "乘数寄存器的最低位是 [y]补 的符号走到底了，不计入乘积\n");
    fprintf(out, "[x·y]补 = %s\n", bits);
    fprintf(out, "真值 %s\n", ratio);
    fprintf(out, "结果：补码 %s，真值 %s\n", bits, ratio);
}

int booth_print(FILE *out, const Booth *o) {
    int i;
    print_booth_head(out, o);
    for (i = 0; i <= o->x.n; i++) {
        print_booth_step(out, o, i);
    }
    print_booth_tail(out, o);
    return 0;
}
