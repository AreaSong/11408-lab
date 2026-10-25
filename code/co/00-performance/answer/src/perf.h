#ifndef PERF_H
#define PERF_H

#include <stdbool.h>
#include <stdio.h>

/* 一题里的指令类数。408 这章的表一般是 4 类，留到 16 够用 */
#define PERF_MAX_CLASS 16

/* 下面这些是公式本身，打印函数和单元测试共用，避免两套数对不上 */

double weighted_cpi(const double *ratio, const double *cpi, int n);
double cpu_seconds(long long ic, double cpi, double hz);
double mips_of(double hz, double cpi);
double gips_of(double hz, double cpi);
/* IC 相同：T1/T2 = (CPI1 × f2) / (CPI2 × f1) */
double time_ratio(double cpi1, double hz1, double cpi2, double hz2);
/* 主频不变时，新时间 = 原时间 × IC 倍数 × CPI 倍数 */
double scale_time(double old_s, double ic_factor, double cpi_factor);
/* 只加速一部分：新时间 = (总时间 - 可改进时间) + 可改进时间 / S */
double amdahl_time(double total_s, double part_s, double speedup);
double speedup_of(double old_s, double new_s);
/* value 的单位是 10^from_exp FLOPS，换成 10^to_exp 那个单位 */
double flops_convert(double value, int from_exp, int to_exp);
double mflops_of(double ops, double seconds);

/* 成功返回 0，参数不合法返回 1（原因写在 stderr） */
int perf_basic(FILE *out, double hz, double cpi, long long ic);
int perf_cpi(FILE *out, double hz, long long ic, const double *ratio, const double *cpi, int n);
int perf_scale(FILE *out, double old_s, double ic_factor, double cpi_factor);
int perf_amdahl(FILE *out, double total_s, double part_s, double speedup,
                bool from_percent, double percent);
int perf_ratio(FILE *out, double hz1, double cpi1, double hz2, double cpi2);
int perf_flops(FILE *out, double value, int exp10);
int perf_rate(FILE *out, double ops, double seconds);

#endif
