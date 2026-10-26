#include "amdahl.h"

#include "frac.h"

Amdahl amdahl_of(int pct, int n) {
    Amdahl a;
    long long den = (long long)n * (100 - pct) + pct;
    frac_reduce(100LL * n, den, &a.speed_num, &a.speed_den);
    frac_reduce(100, den, &a.eff_num, &a.eff_den);
    a.serial_pct = 100 - pct;
    a.limit_infinite = a.serial_pct == 0;
    if (a.limit_infinite) {
        a.limit_num = 0;
        a.limit_den = 1;
    } else {
        frac_reduce(100, a.serial_pct, &a.limit_num, &a.limit_den);
    }
    return a;
}

static void write_step(FILE *fp, long long num, long long den, long long rn, long long rd) {
    fprintf(fp, "%lld/%lld", num, den);
    if (num != rn || den != rd || rd == 1) {
        fputs(" = ", fp);
        frac_write(fp, rn, rd);
    }
}

static void show_limit(FILE *fp, const Amdahl *a) {
    if (a->limit_infinite) {
        fputs("串行部分为 0，核数增加时加速比没有 Amdahl 上界\n", fp);
        return;
    }
    fprintf(fp, "n 趋于无穷时，加速比趋于 1/(1-p) = 100/%d = ", a->serial_pct);
    frac_write(fp, a->limit_num, a->limit_den);
    fputc('\n', fp);
}

static void show_result(FILE *fp, const Amdahl *a) {
    fputs("结果：加速比 ", fp);
    frac_write(fp, a->speed_num, a->speed_den);
    fputs("，效率 ", fp);
    frac_write(fp, a->eff_num, a->eff_den);
    fputs("，极限 ", fp);
    if (a->limit_infinite) {
        fputs("无穷\n", fp);
        return;
    }
    frac_write(fp, a->limit_num, a->limit_den);
    fputc('\n', fp);
}

void amdahl_show(FILE *fp, int pct, int n) {
    Amdahl a = amdahl_of(pct, n);
    long long raw_num = 100LL * n;
    long long raw_den = (long long)n * (100 - pct) + pct;
    fputs("【Amdahl · 多核加速比】\n", fp);
    fprintf(fp, "可并行部分 p = %d%%\n", pct);
    fprintf(fp, "串行部分 1-p = %d%%\n", 100 - pct);
    fprintf(fp, "处理器个数 n = %d 个\n", n);
    fputs("第 00 关：新时间 = 不可改进部分 + 可改进部分 / S。这里可改进部分分给 n 个核，S 就取 n\n", fp);
    fputs("加速比 = 1 / ((1-p) + p/n) = (100×n) / (n×(100-p) + p) = ", fp);
    write_step(fp, raw_num, raw_den, a.speed_num, a.speed_den);
    fputc('\n', fp);
    fputs("效率 E = 加速比 / n = ", fp);
    frac_write(fp, a.eff_num, a.eff_den);
    fputc('\n', fp);
    show_limit(fp, &a);
    show_result(fp, &a);
}

static void show_one_core(FILE *fp, int pct, int n) {
    Amdahl a = amdahl_of(pct, n);
    fprintf(fp, "%d 个核：加速比 ", n);
    frac_write(fp, a.speed_num, a.speed_den);
    fputs("，效率 ", fp);
    frac_write(fp, a.eff_num, a.eff_den);
    fputc('\n', fp);
}

void amdahl_table(FILE *fp, int pct) {
    static const int cores[] = {1, 2, 4, 8};
    Amdahl limit = amdahl_of(pct, 1);
    int i;
    fputs("【Amdahl · 1、2、4、8 个核】\n", fp);
    fprintf(fp, "可并行部分 p = %d%%，串行部分 = %d%%\n", pct, 100 - pct);
    fputs("加速比不会等于核数，除非串行部分是 0\n", fp);
    for (i = 0; i < 4; i++) {
        show_one_core(fp, pct, cores[i]);
    }
    show_limit(fp, &limit);
    fputs("结果：", fp);
    for (i = 0; i < 4; i++) {
        Amdahl a = amdahl_of(pct, cores[i]);
        if (i > 0) {
            fputs("；", fp);
        }
        fprintf(fp, "%d 核 ", cores[i]);
        frac_write(fp, a.speed_num, a.speed_den);
    }
    fputc('\n', fp);
}
