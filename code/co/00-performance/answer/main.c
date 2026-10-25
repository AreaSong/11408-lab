/*
 * 性能计算器的入口。每次读一条命令，读到文件结束为止。
 * 算式和单位在 src/perf.c 里打印，这里只负责把输入拆开。
 */
#include <stdio.h>
#include <string.h>

#include "format.h"
#include "perf.h"
#include "prompt.h"

static int run_basic(void) {
    double value = 0;
    double cpi = 0;
    double hz = 0;
    char unit[16];
    long long ic = 0;
    prompt("请输入「主频 单位 CPI 指令条数」，例如 1.5 GHz 1.2 500000：");
    if (scanf("%lf %15s %lf %lld", &value, unit, &cpi, &ic) != 4) {
        fprintf(stderr, "basic 需要：主频 单位 CPI 指令条数\n");
        return 1;
    }
    if (!parse_hz(value, unit, &hz)) {
        fprintf(stderr, "无法识别的主频单位：%s（用 Hz、kHz、MHz、GHz）\n", unit);
        return 1;
    }
    return perf_basic(stdout, hz, cpi, ic);
}

static int run_cpi(void) {
    double value = 0;
    double hz = 0;
    double ratio[PERF_MAX_CLASS];
    double cpi[PERF_MAX_CLASS];
    char unit[16];
    long long ic = 0;
    int n = 0;
    int i;
    prompt("请输入「主频 单位 指令条数 类数」，指令条数未知填 0：");
    if (scanf("%lf %15s %lld %d", &value, unit, &ic, &n) != 4) {
        fprintf(stderr, "cpi 需要：主频 单位 指令条数 类数\n");
        return 1;
    }
    if (!parse_hz(value, unit, &hz)) {
        fprintf(stderr, "无法识别的主频单位：%s（用 Hz、kHz、MHz、GHz）\n", unit);
        return 1;
    }
    if (n < 1 || n > PERF_MAX_CLASS) {
        fprintf(stderr, "类数要在 1 到 %d 之间\n", PERF_MAX_CLASS);
        return 1;
    }
    for (i = 0; i < n; i++) {
        prompt("请输入第 %d 类的「占比 CPI」（占比用小数，80%% 写成 0.8）：", i + 1);
        if (scanf("%lf %lf", &ratio[i], &cpi[i]) != 2) {
            fprintf(stderr, "第 %d 类的占比和 CPI 没读全\n", i + 1);
            return 1;
        }
    }
    return perf_cpi(stdout, hz, ic, ratio, cpi, n);
}

static int run_scale(void) {
    double value = 0;
    double seconds = 0;
    double ic_factor = 0;
    double cpi_factor = 0;
    char unit[16];
    prompt("请输入「原时间 单位 IC倍数 CPI倍数」，例如 20 s 0.7 1.2：");
    if (scanf("%lf %15s %lf %lf", &value, unit, &ic_factor, &cpi_factor) != 4) {
        fprintf(stderr, "scale 需要：原时间 单位 IC倍数 CPI倍数\n");
        return 1;
    }
    if (!parse_seconds(value, unit, &seconds)) {
        fprintf(stderr, "无法识别的时间单位：%s（用 s、ms、us、ns）\n", unit);
        return 1;
    }
    return perf_scale(stdout, seconds, ic_factor, cpi_factor);
}

static int run_amdahl(void) {
    double total = 0;
    double part = 0;
    double total_s = 0;
    double part_s = 0;
    double speedup = 0;
    double percent = 0;
    char total_unit[16];
    char part_unit[16];
    char speed[32];
    bool from_percent = false;
    prompt("请输入「总时间 单位 可改进时间 单位 加速比」，例如 100 s 90 s +50%%：");
    if (scanf("%lf %15s %lf %15s %31s", &total, total_unit, &part, part_unit, speed) != 5) {
        fprintf(stderr, "amdahl 需要：总时间 单位 可改进时间 单位 加速比\n");
        return 1;
    }
    if (!parse_seconds(total, total_unit, &total_s) || !parse_seconds(part, part_unit, &part_s)) {
        fprintf(stderr, "无法识别的时间单位（用 s、ms、us、ns）\n");
        return 1;
    }
    if (!parse_speedup(speed, &speedup, &from_percent, &percent)) {
        fprintf(stderr, "加速比写成正数（如 1.5）或「+50%%」这种提高的百分比\n");
        return 1;
    }
    return perf_amdahl(stdout, total_s, part_s, speedup, from_percent, percent);
}

static int run_ratio(void) {
    double v1 = 0;
    double v2 = 0;
    double hz1 = 0;
    double hz2 = 0;
    double cpi1 = 0;
    double cpi2 = 0;
    char u1[16];
    char u2[16];
    prompt("请输入「f1 单位 CPI1 f2 单位 CPI2」，例如 1.5 GHz 2 1.2 GHz 1：");
    if (scanf("%lf %15s %lf %lf %15s %lf", &v1, u1, &cpi1, &v2, u2, &cpi2) != 6) {
        fprintf(stderr, "ratio 需要：f1 单位 CPI1 f2 单位 CPI2\n");
        return 1;
    }
    if (!parse_hz(v1, u1, &hz1) || !parse_hz(v2, u2, &hz2)) {
        fprintf(stderr, "无法识别的主频单位（用 Hz、kHz、MHz、GHz）\n");
        return 1;
    }
    return perf_ratio(stdout, hz1, cpi1, hz2, cpi2);
}

static int run_flops(void) {
    double value = 0;
    int exp10 = 0;
    char unit[16];
    prompt("请输入「数值 单位」，例如 3.6 GFLOPS：");
    if (scanf("%lf %15s", &value, unit) != 2) {
        fprintf(stderr, "flops 需要：数值 单位\n");
        return 1;
    }
    if (!parse_flops_unit(unit, &exp10)) {
        fprintf(stderr, "单位要用 MFLOPS、GFLOPS、TFLOPS、PFLOPS、EFLOPS、ZFLOPS\n");
        return 1;
    }
    return perf_flops(stdout, value, exp10);
}

static int run_rate(void) {
    double ops = 0;
    double value = 0;
    double seconds = 0;
    char unit[16];
    prompt("请输入「浮点运算次数 时间 单位」，例如 20000000 0.5 s：");
    if (scanf("%lf %lf %15s", &ops, &value, unit) != 3) {
        fprintf(stderr, "rate 需要：浮点运算次数 时间 单位\n");
        return 1;
    }
    if (!parse_seconds(value, unit, &seconds)) {
        fprintf(stderr, "无法识别的时间单位：%s（用 s、ms、us、ns）\n", unit);
        return 1;
    }
    return perf_rate(stdout, ops, seconds);
}

int main(void) {
    char cmd[32];
    for (;;) {
        int rc;
        prompt("请输入命令（basic / cpi / scale / amdahl / ratio / flops / rate，Ctrl+D 结束）：");
        if (scanf("%31s", cmd) != 1) {
            break;
        }
        if (strcmp(cmd, "basic") == 0) {
            rc = run_basic();
        } else if (strcmp(cmd, "cpi") == 0) {
            rc = run_cpi();
        } else if (strcmp(cmd, "scale") == 0) {
            rc = run_scale();
        } else if (strcmp(cmd, "amdahl") == 0) {
            rc = run_amdahl();
        } else if (strcmp(cmd, "ratio") == 0) {
            rc = run_ratio();
        } else if (strcmp(cmd, "flops") == 0) {
            rc = run_flops();
        } else if (strcmp(cmd, "rate") == 0) {
            rc = run_rate();
        } else {
            fprintf(stderr, "未知命令：%s\n", cmd);
            return 1;
        }
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}
