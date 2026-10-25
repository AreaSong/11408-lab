#include "dram.h"

#include "unit.h"

#include <math.h>
#include <stdio.h>

static void layout_of(int row_bits, int total_bits, DramLayout *out) {
    out->row_bits = row_bits;
    out->col_bits = total_bits - row_bits;
    out->addr_pins = row_bits > out->col_bits ? row_bits : out->col_bits;
    out->rows = 1ull << row_bits;
    out->cols = 1ull << out->col_bits;
}

int dram_layout(uint64_t cells, DramLayout *best, DramLayout *other, int *has_other) {
    int total = exact_log2_u64(cells);
    int row_bits;
    if (total < 0 || best == NULL || has_other == NULL) {
        return 1;
    }
    layout_of(0, total, best);
    *has_other = 0;
    for (row_bits = 1; row_bits <= total; row_bits++) {
        DramLayout candidate;
        layout_of(row_bits, total, &candidate);
        if (candidate.addr_pins < best->addr_pins) {
            *best = candidate;
            *has_other = 0;
        } else if (candidate.addr_pins == best->addr_pins && !*has_other && other != NULL) {
            *other = candidate;
            *has_other = 1;
        }
    }
    return 0;
}

int refresh_plan(uint64_t rows, double interval_s, double cycle_s, RefreshPlan *out) {
    double quot;
    double rounded;
    if (rows == 0 || !(interval_s > 0) || !(cycle_s > 0) || out == NULL) {
        return 1;
    }
    quot = interval_s / cycle_s;
    rounded = round(quot);
    if (rounded < 1 || fabs(quot - rounded) > 1e-6) {
        return 1;
    }
    out->slots = (uint64_t)rounded;
    out->dead_s = (double)rows * cycle_s;
    out->dead_fits = out->dead_s < interval_s;
    out->dead_ratio = out->dead_s / interval_s;
    out->rw_slots = out->slots > rows ? out->slots - rows : 0;
    out->sys_cycle_s = cycle_s * 2.0;
    out->sweep_s = (double)rows * out->sys_cycle_s;
    out->async_gap_s = interval_s / (double)rows;
    return 0;
}

static const char *fit_unit(double seconds) {
    if (fabs(seconds) * 1e3 >= 1.0) {
        return "ms";
    }
    /* 0.5 us 写成 500 ns 和教材对不上，微秒档从 0.1 us 起 */
    if (fabs(seconds) * 1e6 >= 0.1) {
        return "us";
    }
    return "ns";
}

static void print_layout(FILE *out, const char *title, const DramLayout *lay) {
    fprintf(out, "%s：%llu 行 × %llu 列，行地址 %d 位，列地址 %d 位，地址引脚 %d\n", title,
            (unsigned long long)lay->rows, (unsigned long long)lay->cols, lay->row_bits,
            lay->col_bits, lay->addr_pins);
}

int show_dram(FILE *out, uint64_t cells, int data_bits) {
    DramLayout best;
    DramLayout other;
    int has_other = 0;
    char count[96];
    int plain = exact_log2_u64(cells);
    if (data_bits <= 0 || dram_layout(cells, &best, &other, &has_other) != 0) {
        fprintf(stderr, "dram 要求单元数是 2 的幂，数据位数是正整数\n");
        return 1;
    }
    fmt_bin_count(count, sizeof count, cells);
    fprintf(out, "【DRAM 行列复用】\n");
    fprintf(out, "单元数 = %s，数据位数 = %d\n", count, data_bits);
    fprintf(out, "行地址和列地址分时复用同一组引脚：先 RAS 送行，再 CAS 送列\n");
    fprintf(out, "地址引脚 = max(行地址位数, 列地址位数)，不是两者相加\n");
    fprintf(out, "引脚要少，行和列就尽量接近；刷新按行做，引脚一样多时取行更少的\n");
    print_layout(out, "采用", &best);
    if (has_other) {
        print_layout(out, "引脚同样少、但行更多的分法", &other);
    }
    fprintf(out, "数据引脚 = %d，不参加复用\n", data_bits);
    fprintf(out, "地址不复用时地址引脚是 %d，加上数据引脚是 %d\n", plain, plain + data_bits);
    fprintf(out, "结果：r = %llu，c = %llu，地址引脚 = %d，数据引脚 = %d，地址加数据 = %d\n",
            (unsigned long long)best.rows, (unsigned long long)best.cols, best.addr_pins, data_bits,
            best.addr_pins + data_bits);
    return 0;
}

int show_refresh(FILE *out, uint64_t rows, double interval_s, double cycle_s) {
    RefreshPlan plan;
    char interval[64], cycle[64], dead[64], sys[64], sweep[64], gap[64], ratio[64];
    if (refresh_plan(rows, interval_s, cycle_s, &plan) != 0) {
        fprintf(stderr, "refresh 的行数、刷新间隔或存取周期不合法，间隔里要能数出整数个存取周期\n");
        return 1;
    }
    fmt_time_unit(interval, sizeof interval, interval_s, fit_unit(interval_s));
    fmt_time_unit(cycle, sizeof cycle, cycle_s, fit_unit(cycle_s));
    fmt_time_unit(dead, sizeof dead, plan.dead_s, fit_unit(plan.dead_s));
    fmt_time_unit(sys, sizeof sys, plan.sys_cycle_s, fit_unit(plan.sys_cycle_s));
    fmt_time_unit(sweep, sizeof sweep, plan.sweep_s, fit_unit(plan.sweep_s));
    fmt_time_unit(gap, sizeof gap, plan.async_gap_s, fit_unit(plan.async_gap_s));
    fmt_num(ratio, sizeof ratio, plan.dead_ratio * 100.0);
    fprintf(out, "【DRAM 三种刷新】%llu 行，最大刷新间隔 %s，存取周期 %s\n",
            (unsigned long long)rows, interval, cycle);
    fprintf(out, "刷新只送行地址，一行的时间和一次存取相同。间隔里有 %llu 个存取周期\n",
            (unsigned long long)plan.slots);
    fprintf(out, "集中刷新：间隔末尾连续刷完。死时间 = %llu × 存取周期 = %s\n",
            (unsigned long long)rows, dead);
    fprintf(out, "死时间率 = %s%%，能读写的周期 = %llu 个。这段时间 CPU 不能访存\n", ratio,
            (unsigned long long)plan.rw_slots);
    fprintf(out, "分散刷新：每次读写后面紧跟一次刷新。系统存取周期 = %s，没有集中死区\n", sys);
    fprintf(out, "全部行刷完只要 %s，比最大间隔勤得多\n", sweep);
    fprintf(out, "异步刷新：把最大间隔均分到每一行。相邻两行间隔 = %s，每次只停一个存取周期\n",
            gap);
    fprintf(out, "这一次停顿可以放在译码阶段，CPU 这时本来就不访存\n");
    if (!plan.dead_fits) {
        fprintf(out, "集中刷新的死时间已经超过最大间隔，信息保不住\n");
    }
    fprintf(out, "结果：集中死时间 = %s，分散系统存取周期 = %s，异步行间隔 = %s\n", dead, sys, gap);
    return 0;
}
