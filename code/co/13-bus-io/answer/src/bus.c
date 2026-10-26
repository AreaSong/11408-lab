#include "bus.h"

#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "show.h"

double bw_bps(double hz, double clocks, double beats, double bits, double duplex, double channels) {
    return hz * (beats / clocks) * (bits / 8.0) * duplex * channels;
}

double txn_seconds(int each, double hz, double times, double addr, double wait, double data,
                   double prep) {
    double once = (addr + wait + data) / hz + prep;
    if (each) {
        return times * once;
    }
    return once;
}

static int bad_positive(double x) {
    return !(x > 0);
}

int bus_print_bw(FILE *out, double hz, double clocks, double beats, double bits, double duplex,
                 double channels) {
    double work;
    double bps;
    char hz_text[96];
    char work_text[96];
    char nclock[64];
    char nbeat[64];
    char nbit[64];
    char nbytes[64];
    char ndup[64];
    char nch[64];
    char rate[160];
    char brief[64];
    if (bad_positive(hz) || bad_positive(clocks) || bad_positive(beats) || bad_positive(bits) ||
        bad_positive(duplex) || bad_positive(channels)) {
        fprintf(stderr, "频率、时钟数、传送次数、位宽、双工倍数和通道数都要是正数\n");
        return 1;
    }
    work = hz * beats / clocks;
    bps = bw_bps(hz, clocks, beats, bits, duplex, channels);
    fmt_hz(hz_text, sizeof hz_text, hz);
    fmt_hz(work_text, sizeof work_text, work);
    fmt_num(nclock, sizeof nclock, clocks);
    fmt_num(nbeat, sizeof nbeat, beats);
    fmt_num(nbit, sizeof nbit, bits);
    fmt_num(nbytes, sizeof nbytes, bits / 8.0);
    fmt_num(ndup, sizeof ndup, duplex);
    fmt_num(nch, sizeof nch, channels);
    fmt_rate(rate, sizeof rate, bps);
    fmt_rate_short(brief, sizeof brief, bps);
    fprintf(out, "【总线带宽】\n");
    fprintf(out, "时钟频率 = %s\n", hz_text);
    fprintf(out, "每 %s 个时钟周期传送 %s 次数据，工作频率 = %s\n", nclock, nbeat, work_text);
    fprintf(out, "总线宽度 = %s 位 = %s B，双工倍数 = %s，通道数 = %s\n", nbit, nbytes, ndup, nch);
    fprintf(out, "带宽 = 工作频率 × 一次的字节数 × 双工倍数 × 通道数 = %s\n", rate);
    fprintf(out, "带宽只算数据阶段。地址、裁决和等待不计入，所以它是最大传输率\n");
    fprintf(out, "（带宽里的 M、G 是 10^6、10^9，不是存储容量的 2^20、2^30）\n");
    fprintf(out, "结果：%s\n", brief);
    return 0;
}

static void print_peak(FILE *out, double hz, double data_clocks, double bytes_in_data) {
    char bytes[64];
    char clocks[64];
    char rate[160];
    char brief[64];
    double bps;
    if (!(data_clocks > 0) || !(bytes_in_data > 0)) {
        return;
    }
    bps = bytes_in_data / data_clocks * hz;
    fmt_num(bytes, sizeof bytes, bytes_in_data);
    fmt_num(clocks, sizeof clocks, data_clocks);
    fmt_rate(rate, sizeof rate, bps);
    fmt_rate_short(brief, sizeof brief, bps);
    fprintf(out, "数据阶段 %s B / %s 个时钟，带宽 = %s，即 %s\n", bytes, clocks, rate, brief);
}

static void print_txn_tail(FILE *out, double hz, double data, double data_bytes, const char *bytes,
                           const char *avg_text, const char *all_text) {
    fprintf(out, "这次搬 %s B，平均传输率 = %s\n", bytes, avg_text);
    fprintf(out, "平均传输率把地址和等待算进去了，不等于带宽\n");
    print_peak(out, hz, data, data_bytes);
    fprintf(out, "结果：%s\n", all_text);
}

static int txn_bad(int each, double hz, double times, double addr, double wait, double data,
                   double prep, double bits, double payload) {
    if (bad_positive(hz) || bad_positive(bits) || bad_positive(payload) || addr < 0 || wait < 0 ||
        data < 0 || prep < 0 || (each && bad_positive(times))) {
        fprintf(stderr, "事务的频率、次数、位宽和字节数要是正数，时钟数和准备时间不能为负\n");
        return 1;
    }
    return 0;
}

int bus_print_txn(FILE *out, int each, double hz, double times, double addr, double wait, double data, double prep, double bits, double payload) {
    double reps = each ? times : 1;
    double seconds;
    double once;
    double avg;
    if (txn_bad(each, hz, times, addr, wait, data, prep, bits, payload)) {
        return 1;
    }
    seconds = txn_seconds(each, hz, times, addr, wait, data, prep);
    char hz_text[96];
    char period[64];
    char na[32];
    char nw[32];
    char nd[32];
    char nt[32];
    char prep_text[64];
    char once_text[64];
    char all_text[64];
    char bytes[64];
    char avg_text[64];
    once = seconds / reps;
    fmt_hz(hz_text, sizeof hz_text, hz);
    fmt_time_short(period, sizeof period, 1.0 / hz);
    fmt_num(na, sizeof na, addr);
    fmt_num(nw, sizeof nw, wait);
    fmt_num(nd, sizeof nd, data);
    fmt_num(nt, sizeof nt, reps);
    fmt_time_short(prep_text, sizeof prep_text, prep);
    fmt_time_short(once_text, sizeof once_text, once);
    fmt_time_short(all_text, sizeof all_text, seconds);
    fmt_num(bytes, sizeof bytes, payload);
    avg = payload / seconds;
    fmt_rate_short(avg_text, sizeof avg_text, avg);
    fprintf(out, "【一次总线事务】\n");
    if (each) {
        fprintf(out, "非突发：每次数据都重新送地址，共 %s 次\n", nt);
    } else {
        fprintf(out, "突发：地址只送一次，随后连续送数据\n");
    }
    fprintf(out, "时钟频率 = %s，时钟周期 = %s\n", hz_text, period);
    fprintf(out, "地址 %s 个时钟 + 等待 %s 个时钟 + 数据 %s 个时钟，准备 %s\n", na, nw, nd,
            prep_text);
    if (each) {
        fprintf(out, "每次 %s，%s 次合计 %s\n", once_text, nt, all_text);
    } else {
        fprintf(out, "事务时间 = %s\n", all_text);
    }
    print_txn_tail(out, hz, data, each ? payload / reps : payload, bytes, avg_text, all_text);
    return 0;
}

static int run_bw(void) {
    double freq = 0;
    double clocks = 0;
    double beats = 0;
    double bits = 0;
    double duplex = 0;
    double channels = 0;
    double hz = 0;
    char unit[16];
    prompt("请输入「频率 单位 时钟数 传送次数 位宽 双工 通道」，例如 66 MHz 1 2 32 1 1：");
    if (scanf("%lf %15s %lf %lf %lf %lf %lf", &freq, unit, &clocks, &beats, &bits, &duplex,
              &channels) != 7) {
        fprintf(stderr, "bw 需要：频率 单位 时钟数 传送次数 位宽 双工 通道\n");
        return 1;
    }
    if (!parse_hz(freq, unit, &hz)) {
        fprintf(stderr, "无法识别的频率单位：%s（用 Hz、kHz、MHz、GHz）\n", unit);
        return 1;
    }
    return bus_print_bw(stdout, hz, clocks, beats, bits, duplex, channels);
}

static int run_txn(void) {
    double freq = 0;
    double times = 1;
    double addr = 0;
    double wait = 0;
    double data = 0;
    double prep = 0;
    double prep_s = 0;
    double bits = 0;
    double payload = 0;
    double hz = 0;
    char mode[16];
    char funit[16];
    char punit[16];
    int each;
    prompt("请输入 burst 或 each：");
    if (scanf("%15s", mode) != 1) {
        fprintf(stderr, "txn 需要 burst 或 each\n");
        return 1;
    }
    each = strcmp(mode, "each") == 0;
    if (!each && strcmp(mode, "burst") != 0) {
        fprintf(stderr, "txn 的方式只能是 burst 或 each\n");
        return 1;
    }
    if (each) {
        prompt("请输入「频率 单位 次数 地址时钟 等待时钟 数据时钟 准备 单位 位宽 字节」：");
        if (scanf("%lf %15s %lf %lf %lf %lf %lf %15s %lf %lf", &freq, funit, &times, &addr, &wait,
                  &data, &prep, punit, &bits, &payload) != 10) {
            fprintf(stderr, "txn each 的参数没读全\n");
            return 1;
        }
    } else {
        prompt("请输入「频率 单位 地址时钟 等待时钟 数据时钟 准备 单位 位宽 字节」：");
        if (scanf("%lf %15s %lf %lf %lf %lf %15s %lf %lf", &freq, funit, &addr, &wait, &data, &prep,
                  punit, &bits, &payload) != 9) {
            fprintf(stderr, "txn burst 的参数没读全\n");
            return 1;
        }
    }
    if (!parse_hz(freq, funit, &hz) || !parse_seconds(prep, punit, &prep_s)) {
        fprintf(stderr, "频率用 Hz/kHz/MHz/GHz，准备时间用 s/ms/us/ns\n");
        return 1;
    }
    return bus_print_txn(stdout, each, hz, times, addr, wait, data, prep_s, bits, payload);
}

int bus_command(const char *cmd) {
    if (strcmp(cmd, "bw") == 0) {
        return run_bw();
    }
    if (strcmp(cmd, "txn") == 0) {
        return run_txn();
    }
    return -1;
}
