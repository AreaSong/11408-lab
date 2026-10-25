#include "divide.h"

static void write_got(char *dst, size_t n, const Divide *d, int count) {
    int i;
    if (n < (size_t)count + 3) {
        if (n > 0) {
            dst[0] = '\0';
        }
        return;
    }
    dst[0] = '0';
    dst[1] = '.';
    for (i = 0; i < count; i++) {
        dst[2 + i] = d->step[i].qbit ? '1' : '0';
    }
    dst[2 + count] = '\0';
}

static int print_divide_bad(FILE *out, const Divide *o) {
    char xs[24];
    char ys[24];
    frac_text(xs, sizeof xs, o->x);
    frac_text(ys, sizeof ys, o->y);
    fprintf(out, "【原码加减交替】x = %s，y = %s，数值位 n = %d\n", xs, ys, o->x.n);
    if (o->kind == 2) {
        fprintf(out, "除数为 0。商和余数都没有定义，运算停在这里\n");
        fprintf(out, "结果：除数为 0\n");
        return 0;
    }
    fprintf(out, "|x| 的数值是 %u，|y| 的数值是 %u。|x| 不小于 |y|\n", o->x.mag, o->y.mag);
    fprintf(out, "第一次试减的余数会 ≥ 0，说明商至少是 1。n 位定点小数放不下\n");
    fprintf(out, "结果：商溢出\n");
    return 0;
}

static void print_divide_head(FILE *out, const Divide *o) {
    char xs[24];
    char ys[24];
    char xm[24];
    char ym[24];
    char neg[24];
    char trial[24];
    int n = o->x.n;
    frac_text(xs, sizeof xs, o->x);
    frac_text(ys, sizeof ys, o->y);
    frac_dot(xm, sizeof xm, o->x.mag, 1, n);
    frac_dot(ym, sizeof ym, o->y.mag, 1, n);
    frac_dot(neg, sizeof neg, o->neg_y, 1, n);
    frac_dot(trial, sizeof trial, o->trial, 1, n);
    fprintf(out, "【原码加减交替】x = %s，y = %s，数值位 n = %d\n", xs, ys, n);
    fprintf(out, "商的符号 = %d ⊕ %d = %d。余数与被除数同号\n", o->x.sign, o->y.sign, o->qsign);
    fprintf(out, "|x| = %s，|y| = %s，[-|y|]补 = %s\n", xm, ym, neg);
    fprintf(out, "R 放余数，Q 放商，Y 放 |y|。每步先左移，再按左移前的余数符号加或减\n");
    fprintf(out, "第一次试减只判断溢出。这一位不写入商，余数为负也不加回去\n");
    fprintf(out, "|x| + [-|y|]补 = %s + %s = %s，为负，没有溢出\n", xm, neg, trial);
}

static void print_divide_step(FILE *out, const Divide *o, int i) {
    const DivStep *s = &o->step[i];
    char shifted[24];
    char rem[24];
    char ytext[24];
    char neg[24];
    char got[16];
    int n = o->x.n;
    frac_dot(shifted, sizeof shifted, s->shifted, 1, n);
    frac_dot(rem, sizeof rem, s->rem, 1, n);
    frac_dot(ytext, sizeof ytext, o->y.mag, 1, n);
    frac_dot(neg, sizeof neg, o->neg_y, 1, n);
    write_got(got, sizeof got, o, i + 1);
    if (s->prev_neg) {
        fprintf(out, "步 %d  上一步余数为负，左移后加 |y|\n", i + 1);
    } else {
        fprintf(out, "步 %d  上一步余数为正，左移后减 |y|\n", i + 1);
    }
    fprintf(out, "左移 %s\n", shifted);
    if (s->sign_flipped) {
        fprintf(out, "左移后的最高位和左移前的符号不一样。加还是减，看左移之前那一位\n");
    }
    if (s->add) {
        fprintf(out, "+ |y| %s\n", ytext);
    } else {
        fprintf(out, "+ [-|y|]补 %s\n", neg);
    }
    fprintf(out, "余数 %s，%s，商上 %d\n", rem, s->qbit ? "够减" : "不够减", s->qbit);
    fprintf(out, "已得商位 %s\n", got);
}

static void print_divide_tail(FILE *out, const Divide *o) {
    char before[24];
    char after[24];
    char ratio[40];
    char qtext[24];
    Frac qf;
    Frac mr;
    long long xv = frac_num(o->x);
    long long yv = frac_num(o->y);
    long long qv = o->qsign ? -(long long)o->q : (long long)o->q;
    int n = o->x.n;
    frac_dot(before, sizeof before, o->rem_before_fix, 1, n);
    qf.n = n;
    qf.sign = o->qsign;
    qf.mag = o->q;
    mr.n = n;
    mr.sign = (o->x.sign && o->machine_rem != 0) ? 1 : 0;
    mr.mag = o->machine_rem;
    frac_text(qtext, sizeof qtext, qf);
    frac_text(after, sizeof after, mr);
    frac_ratio(ratio, sizeof ratio, o->rem_num, 2 * n);
    if (o->corrected) {
        fprintf(out, "末位余数为负。后面没有下一次左移，这一次要加回 |y|，商不变\n");
        fprintf(out, "加回之前 %s，加回之后机器余数 %s\n", before, after);
    } else {
        fprintf(out, "末位余数为正，不用加回 |y|\n");
    }
    fprintf(out, "做了 %d 次左移。机器余数被左移过 n 次，真实余数 = 机器余数 × 2^(-n)\n", n);
    fprintf(out, "机器余数 %s，真实余数 %s\n", after, ratio);
    fprintf(out, "验算：商的分子 %lld × 除数的分子 %lld + 余数的分子 %lld = 被除数的分子 %lld × 2^%d\n", qv, yv,
            o->rem_num, xv, n);
    fprintf(out, "结果：商 %s，余数 %s\n", qtext, ratio);
}

int divide_print(FILE *out, const Divide *o) {
    int i;
    if (o->kind != 0) {
        return print_divide_bad(out, o);
    }
    print_divide_head(out, o);
    for (i = 0; i < o->x.n; i++) {
        print_divide_step(out, o, i);
    }
    print_divide_tail(out, o);
    return 0;
}
