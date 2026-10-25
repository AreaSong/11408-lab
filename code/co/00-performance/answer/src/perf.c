#include "perf.h"

#include "format.h"

#include <math.h>
#include <stdio.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static double pow10(int exp) {
    double x = 1;
    int i;
    int n = exp < 0 ? -exp : exp;
    for (i = 0; i < n; i++) {
        x = exp < 0 ? x / 10.0 : x * 10.0;
    }
    return x;
}

double weighted_cpi(const double *ratio, const double *cpi, int n) {
    double sum = 0;
    int i;
    for (i = 0; i < n; i++) {
        /* 先收成十进制再相加，0.1×4 才稳定等于 0.4 */
        sum += snap(ratio[i] * cpi[i]);
    }
    return snap(sum);
}

double cpu_seconds(long long ic, double cpi, double hz) {
    return snap(snap((double)ic * cpi) / hz);
}

double mips_of(double hz, double cpi) {
    /* M = 10^6。用 2^20 会和标准答案差一截，2013 年的 400 就是这样算出来的 */
    return snap(hz / (cpi * 1e6));
}

double gips_of(double hz, double cpi) {
    return snap((hz / cpi) / 1e9);
}

double time_ratio(double cpi1, double hz1, double cpi2, double hz2) {
    return snap((cpi1 * hz2) / (cpi2 * hz1));
}

double scale_time(double old_s, double ic_factor, double cpi_factor) {
    return snap(old_s * ic_factor * cpi_factor);
}

double amdahl_time(double total_s, double part_s, double speedup) {
    double rest = snap(total_s - part_s);
    return snap(rest + snap(part_s / speedup));
}

double speedup_of(double old_s, double new_s) {
    return snap(old_s / new_s);
}

double flops_convert(double value, int from_exp, int to_exp) {
    return snap(value * pow10(from_exp - to_exp));
}

double mflops_of(double ops, double seconds) {
    return snap(ops / (seconds * 1e6));
}

static void print_rates(FILE *out, double hz, double cpi, long long ic, double seconds) {
    char hz_b[64], cpi_b[64], den_b[64], ips_b[64], mips_b[64], gips_b[64], ipc_b[64];
    double ips = snap(hz / cpi);
    double mips = mips_of(hz, cpi);
    double gips = gips_of(hz, cpi);

    fmt_num(hz_b, sizeof hz_b, hz);
    fmt_num(cpi_b, sizeof cpi_b, cpi);
    fmt_num(den_b, sizeof den_b, snap(cpi * 1e6));
    fmt_num(ips_b, sizeof ips_b, ips);
    fmt_num(mips_b, sizeof mips_b, mips);
    fmt_num(gips_b, sizeof gips_b, gips);
    fmt_num(ipc_b, sizeof ipc_b, snap(1.0 / cpi));
    fprintf(out, "指令执行速度 = f / CPI = %s / %s = %s 条/s = %s MIPS = %s GIPS\n",
            hz_b, cpi_b, ips_b, mips_b, gips_b);
    fprintf(out, "MIPS = f / (CPI × 10^6) = %s / %s = %s\n", hz_b, den_b, mips_b);
    if (ic > 0 && seconds > 0) {
        char ic_b[64], sec_b[64], alt_b[64];
        fmt_num(ic_b, sizeof ic_b, (double)ic);
        fmt_num(sec_b, sizeof sec_b, seconds);
        fmt_num(alt_b, sizeof alt_b, snap((double)ic / (seconds * 1e6)));
        fprintf(out, "核对：MIPS = IC / (CPU 时间 × 10^6) = %s / (%s × 10^6) = %s\n",
                ic_b, sec_b, alt_b);
    }
    fprintf(out, "IPC = 1/CPI = %s 条/周期\n", ipc_b);
    fprintf(out, "（性能指标里的 M、G 是 10^6、10^9，不是存储容量的 2^20、2^30）\n");
}

static void print_cpu_time(FILE *out, long long ic, double cpi, double hz) {
    char ic_b[64], cpi_b[64], cyc_b[64], hz_b[64], sec_b[160];
    double cycles = snap((double)ic * cpi);
    double seconds = cpu_seconds(ic, cpi, hz);

    fmt_num(ic_b, sizeof ic_b, (double)ic);
    fmt_num(cpi_b, sizeof cpi_b, cpi);
    fmt_num(cyc_b, sizeof cyc_b, cycles);
    fmt_num(hz_b, sizeof hz_b, hz);
    fmt_seconds(sec_b, sizeof sec_b, seconds);
    fprintf(out, "总时钟周期数 = IC × CPI = %s × %s = %s 周期\n", ic_b, cpi_b, cyc_b);
    fprintf(out, "CPU 时间 = 总时钟周期数 / f = %s / %s = %s\n", cyc_b, hz_b, sec_b);
}

static int print_classes(FILE *out, const double *ratio, const double *cpi, int n, double *avg) {
    char expr[512];
    size_t used = 0;
    double sum_r = 0;
    double sum_c = 0;
    int i;

    expr[0] = '\0';
    for (i = 0; i < n; i++) {
        char rb[64], cb[64], tb[64];
        double term = snap(ratio[i] * cpi[i]);
        int wrote;
        sum_r += ratio[i];
        sum_c += term;
        fmt_num(rb, sizeof rb, ratio[i]);
        fmt_num(cb, sizeof cb, cpi[i]);
        fmt_num(tb, sizeof tb, term);
        fprintf(out, "第 %c 类：占比 %s × CPI %s = %s\n", 'A' + i, rb, cb, tb);
        wrote = snprintf(expr + used, sizeof expr - used, "%s%s", used ? " + " : "", tb);
        if (wrote < 0 || (size_t)wrote >= sizeof expr - used) {
            return fail("指令类太多，算式放不下");
        }
        used += (size_t)wrote;
    }
    *avg = snap(sum_c);
    {
        char rs[64], av[64];
        fmt_num(rs, sizeof rs, snap(sum_r));
        fmt_num(av, sizeof av, *avg);
        fprintf(out, "占比之和 = %s\n", rs);
        if (fabs(snap(sum_r) - 1.0) > 1e-9) {
            fprintf(out, "注意：占比之和不等于 1，仍按加权求和计算\n");
        }
        fprintf(out, "平均 CPI = %s = %s 周期/条\n", expr, av);
    }
    return 0;
}

int perf_basic(FILE *out, double hz, double cpi, long long ic) {
    char fbuf[128], tbuf[160], cpi_b[64], ic_b[64], gips_b[64], sec_b[160];
    double seconds;
    if (!(hz > 0) || !(cpi > 0) || ic < 0) {
        return fail("basic：主频和 CPI 必须为正，指令条数不能为负");
    }
    seconds = cpu_seconds(ic, cpi, hz);
    fmt_hz(fbuf, sizeof fbuf, hz);
    fmt_seconds(tbuf, sizeof tbuf, 1.0 / hz);
    fmt_num(cpi_b, sizeof cpi_b, cpi);
    fmt_num(ic_b, sizeof ic_b, (double)ic);
    fmt_num(gips_b, sizeof gips_b, gips_of(hz, cpi));
    fmt_seconds(sec_b, sizeof sec_b, seconds);
    fprintf(out, "【基本公式】\n");
    fprintf(out, "主频 f = %s\n", fbuf);
    fprintf(out, "时钟周期 T = 1/f = %s\n", tbuf);
    fprintf(out, "CPI = %s 周期/条\n", cpi_b);
    fprintf(out, "指令条数 IC = %s 条\n", ic_b);
    print_cpu_time(out, ic, cpi, hz);
    print_rates(out, hz, cpi, ic, seconds);
    fprintf(out, "结果：指令执行速度 = %s GIPS，CPU 时间 = %s\n", gips_b, sec_b);
    return 0;
}

int perf_cpi(FILE *out, double hz, long long ic, const double *ratio, const double *cpi, int n) {
    char fbuf[128], av_b[64], sec_b[160], mips_b[64];
    double avg = 0;
    int i;
    if (!(hz > 0) || ic < 0 || n < 1 || n > PERF_MAX_CLASS) {
        return fail("cpi：主频必须为正，类数要在 1 到 16 之间");
    }
    for (i = 0; i < n; i++) {
        if (ratio[i] < 0 || ratio[i] > 1 || !(cpi[i] > 0)) {
            return fail("cpi：占比用 0 到 1 的小数（80% 写成 0.8），CPI 必须为正");
        }
    }
    fprintf(out, "【加权平均 CPI】\n");
    fmt_hz(fbuf, sizeof fbuf, hz);
    fprintf(out, "主频 f = %s\n", fbuf);
    if (ic > 0) {
        char ic_b[64];
        fmt_num(ic_b, sizeof ic_b, (double)ic);
        fprintf(out, "指令条数 IC = %s 条\n", ic_b);
    } else {
        fprintf(out, "指令条数未给出，只算平均 CPI 和 MIPS\n");
    }
    if (print_classes(out, ratio, cpi, n, &avg) != 0) {
        return 1;
    }
    if (!(avg > 0)) {
        return fail("cpi：平均 CPI 必须为正");
    }
    fmt_num(av_b, sizeof av_b, avg);
    fmt_num(mips_b, sizeof mips_b, mips_of(hz, avg));
    if (ic > 0) {
        print_cpu_time(out, ic, avg, hz);
        print_rates(out, hz, avg, ic, cpu_seconds(ic, avg, hz));
        fmt_seconds(sec_b, sizeof sec_b, cpu_seconds(ic, avg, hz));
        fprintf(out, "结果：平均 CPI = %s 周期/条，CPU 时间 = %s，MIPS = %s\n",
                av_b, sec_b, mips_b);
    } else {
        print_rates(out, hz, avg, 0, 0);
        fprintf(out, "结果：平均 CPI = %s 周期/条，MIPS = %s\n", av_b, mips_b);
    }
    return 0;
}

int perf_scale(FILE *out, double old_s, double ic_factor, double cpi_factor) {
    char old_b[160], neu_b[160], ic_b[64], cpi_b[64], sp_b[64];
    double neu;
    double sp;
    if (!(old_s > 0) || !(ic_factor > 0) || !(cpi_factor > 0)) {
        return fail("scale：原时间和两个倍数都必须为正");
    }
    neu = scale_time(old_s, ic_factor, cpi_factor);
    sp = speedup_of(old_s, neu);
    fmt_seconds(old_b, sizeof old_b, old_s);
    fmt_seconds(neu_b, sizeof neu_b, neu);
    fmt_num(ic_b, sizeof ic_b, ic_factor);
    fmt_num(cpi_b, sizeof cpi_b, cpi_factor);
    fmt_num(sp_b, sizeof sp_b, sp);
    fprintf(out, "【加速比 · 指令条数与 CPI 变化】\n");
    fprintf(out, "原执行时间 T = %s\n", old_b);
    fprintf(out, "主频不变，执行时间与 IC × CPI 成正比\n");
    fprintf(out, "指令条数变为原来的 %s 倍，CPI 变为原来的 %s 倍\n", ic_b, cpi_b);
    fprintf(out, "新执行时间 = T × %s × %s = %s\n", ic_b, cpi_b, neu_b);
    fprintf(out, "加速比 = 原时间 / 新时间 = %s / %s = %s\n", old_b, neu_b, sp_b);
    fprintf(out, "结果：执行时间 = %s，加速比 = %s\n", neu_b, sp_b);
    return 0;
}

static void print_amdahl_check(FILE *out, double total_s, double part_s, double speedup) {
    char f_b[64], rest_b[64], part_b[64], den_b[64], sp_b[64], s_b[64];
    double f = snap(part_s / total_s);
    double rest = snap(1.0 - f);
    double piece = snap(f / speedup);
    double den = snap(rest + piece);
    fmt_num(f_b, sizeof f_b, f);
    fmt_num(rest_b, sizeof rest_b, rest);
    fmt_num(part_b, sizeof part_b, piece);
    fmt_num(den_b, sizeof den_b, den);
    fmt_num(s_b, sizeof s_b, speedup);
    fmt_num(sp_b, sizeof sp_b, snap(1.0 / den));
    fprintf(out, "公式核对：1 / ((1 - F) + F / S) = 1 / ((1 - %s) + %s / %s) = 1 / (%s + %s) = 1 / %s = %s\n",
            f_b, f_b, s_b, rest_b, part_b, den_b, sp_b);
}

int perf_amdahl(FILE *out, double total_s, double part_s, double speedup,
                bool from_percent, double percent) {
    char total_b[64], part_b[64], rest_b[64], f_b[64];
    char neu_part_b[64], neu_b[64], sp_b[64], s_b[64], neu_sec[160];
    double rest;
    double neu_part;
    double neu;
    if (!(total_s > 0) || !(part_s >= 0) || part_s - total_s > 1e-9 || !(speedup > 0)) {
        return fail("amdahl：总时间必须为正，可改进部分不能超过总时间，加速比必须为正");
    }
    rest = snap(total_s - part_s);
    neu_part = snap(part_s / speedup);
    neu = amdahl_time(total_s, part_s, speedup);
    fmt_num(total_b, sizeof total_b, total_s);
    fmt_num(part_b, sizeof part_b, part_s);
    fmt_num(rest_b, sizeof rest_b, rest);
    fmt_num(f_b, sizeof f_b, snap(part_s / total_s));
    fmt_num(neu_part_b, sizeof neu_part_b, neu_part);
    fmt_num(neu_b, sizeof neu_b, neu);
    fmt_seconds(neu_sec, sizeof neu_sec, neu);
    fmt_num(s_b, sizeof s_b, speedup);
    fmt_num(sp_b, sizeof sp_b, speedup_of(total_s, neu));
    fprintf(out, "【加速比 · Amdahl 定律】\n");
    fprintf(out, "原执行时间 T = %s s\n", total_b);
    fprintf(out, "可改进部分原时间 = %s s\n", part_b);
    fprintf(out, "不可改进部分时间 = %s - %s = %s s\n", total_b, part_b, rest_b);
    fprintf(out, "可改进部分占比 F = %s / %s = %s\n", part_b, total_b, f_b);
    if (from_percent) {
        char p_b[64];
        fmt_num(p_b, sizeof p_b, percent);
        fprintf(out, "速度提高 %s%%：部件加速比 S = 1 + %s/100 = %s\n", p_b, p_b, s_b);
        fprintf(out, "（提高 p%% 表示性能变为原来的 1+p/100 倍，不是把这部分时间乘 1-p/100）\n");
    } else {
        fprintf(out, "部件加速比 S = %s\n", s_b);
    }
    fprintf(out, "改进后该部分时间 = %s / %s = %s s\n", part_b, s_b, neu_part_b);
    fprintf(out, "改进后总时间 = 不可改进部分 + 可改进部分 / S = %s + %s = %s s\n",
            rest_b, neu_part_b, neu_b);
    fprintf(out, "总加速比 = T / 新时间 = %s / %s = %s\n", total_b, neu_b, sp_b);
    print_amdahl_check(out, total_s, part_s, speedup);
    fprintf(out, "结果：执行时间 = %s，加速比 = %s\n", neu_sec, sp_b);
    return 0;
}

int perf_ratio(FILE *out, double hz1, double cpi1, double hz2, double cpi2) {
    char f1[128], f2[128], c1[64], c2[64], hz1_b[64], hz2_b[64];
    char num_b[64], den_b[64], r_b[64];
    double num;
    double den;
    double ratio;
    if (!(hz1 > 0) || !(hz2 > 0) || !(cpi1 > 0) || !(cpi2 > 0)) {
        return fail("ratio：两台机器的主频和 CPI 都必须为正");
    }
    num = snap(cpi1 * hz2);
    den = snap(cpi2 * hz1);
    ratio = time_ratio(cpi1, hz1, cpi2, hz2);
    fmt_hz(f1, sizeof f1, hz1);
    fmt_hz(f2, sizeof f2, hz2);
    fmt_num(c1, sizeof c1, cpi1);
    fmt_num(c2, sizeof c2, cpi2);
    fmt_num(hz1_b, sizeof hz1_b, hz1);
    fmt_num(hz2_b, sizeof hz2_b, hz2);
    fmt_num(num_b, sizeof num_b, num);
    fmt_num(den_b, sizeof den_b, den);
    fmt_num(r_b, sizeof r_b, ratio);
    fprintf(out, "【两台机器的运行时间之比】\n");
    fprintf(out, "指令条数相同，IC 约掉：T1/T2 = (CPI1 × f2) / (CPI2 × f1)\n");
    fprintf(out, "M1：f1 = %s，CPI1 = %s\n", f1, c1);
    fprintf(out, "M2：f2 = %s，CPI2 = %s\n", f2, c2);
    fprintf(out, "T1/T2 = (%s × %s) / (%s × %s) = %s / %s = %s\n",
            c1, hz2_b, c2, hz1_b, num_b, den_b, r_b);
    fprintf(out, "结果：T1/T2 = %s\n", r_b);
    return 0;
}

int perf_flops(FILE *out, double value, int exp10) {
    char in_b[64], base_b[64];
    int i;
    int count = flops_unit_count();
    if (!(value > 0)) {
        return fail("flops：数值必须为正");
    }
    fmt_num(in_b, sizeof in_b, value);
    fmt_num(base_b, sizeof base_b, flops_convert(value, exp10, 0));
    fprintf(out, "【FLOPS 单位换算】\n");
    fprintf(out, "输入 = %s × 10^%d FLOPS = %s FLOPS\n", in_b, exp10, base_b);
    fprintf(out, "性能前缀 M、G、T、P、E、Z 依次是 10^6 到 10^21，不是 2 的幂：\n");
    for (i = 0; i < count; i++) {
        char v_b[64];
        fmt_num(v_b, sizeof v_b, flops_convert(value, exp10, flops_unit_exp(i)));
        fprintf(out, "%s = %s（10^%d）\n", flops_unit_name(i), v_b, flops_unit_exp(i));
    }
    {
        char mf[64];
        fmt_num(mf, sizeof mf, flops_convert(value, exp10, 6));
        fprintf(out, "结果：%s MFLOPS\n", mf);
    }
    return 0;
}

int perf_rate(FILE *out, double ops, double seconds) {
    char n_b[64], t_b[160], den_b[64], mf_b[64];
    double mf;
    int i;
    if (!(ops > 0) || !(seconds > 0)) {
        return fail("rate：运算次数和执行时间都必须为正");
    }
    mf = mflops_of(ops, seconds);
    fmt_num(n_b, sizeof n_b, ops);
    fmt_seconds(t_b, sizeof t_b, seconds);
    fmt_num(den_b, sizeof den_b, snap(seconds * 1e6));
    fmt_num(mf_b, sizeof mf_b, mf);
    fprintf(out, "【浮点运算速度】\n");
    fprintf(out, "浮点运算次数 N = %s 次\n", n_b);
    fprintf(out, "执行时间 T = %s\n", t_b);
    fprintf(out, "MFLOPS = N / (T × 10^6) = %s / %s = %s\n", n_b, den_b, mf_b);
    fprintf(out, "换算到各级单位：\n");
    for (i = 0; i < flops_unit_count(); i++) {
        char v_b[64];
        fmt_num(v_b, sizeof v_b, flops_convert(mf, 6, flops_unit_exp(i)));
        fprintf(out, "%s = %s（10^%d）\n", flops_unit_name(i), v_b, flops_unit_exp(i));
    }
    fprintf(out, "（MFLOPS 数的是浮点运算，MIPS 数的是指令，两者不能直接比大小）\n");
    fprintf(out, "结果：MFLOPS = %s\n", mf_b);
    return 0;
}
