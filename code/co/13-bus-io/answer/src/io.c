#include "io.h"

#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "show.h"

double share_ratio(double hz, double times, double cycles) {
    return times * cycles / hz;
}

double times_from_rate(double bytes_per_s, double bytes) {
    return bytes_per_s / bytes;
}

int keeps_up(double prepare_s, double service_s) {
    return prepare_s >= service_s;
}

TaskClocks task_clocks(double hz, double gap_s, double cpi, double resp, double insns,
                       double launch, double count) {
    TaskClocks t;
    double prep = gap_s * hz;
    t.total_cycles = (prep + resp + launch * cpi) * count;
    t.cpu_cycles = (resp + insns * cpi) * count;
    t.extra_last = (insns - launch) * cpi;
    return t;
}

long long dma_requests(long long nbytes, long long buf_bits) {
    long long bits;
    if (nbytes <= 0 || buf_bits <= 0) {
        return -1;
    }
    bits = nbytes * 8;
    if (bits % buf_bits != 0) {
        return -1;
    }
    return bits / buf_bits;
}

static int read_hz_unit(double value, const char *unit, double *hz) {
    if (!parse_hz(value, unit, hz)) {
        fprintf(stderr, "无法识别的频率单位：%s（用 Hz、kHz、MHz、GHz）\n", unit);
        return 0;
    }
    return 1;
}

static int read_rate_unit(double value, const char *unit, double *bps) {
    if (!parse_rate(value, unit, bps)) {
        fprintf(stderr, "无法识别的传输率单位：%s（用 B/s、KB/s、MB/s、GB/s）\n", unit);
        return 0;
    }
    return 1;
}

static void print_occupy(FILE *out, double hz, double times, double cycles) {
    char ntimes[64];
    char ncycles[64];
    char used[64];
    char freq[64];
    char pct[64];
    fmt_num(ntimes, sizeof ntimes, times);
    fmt_num(ncycles, sizeof ncycles, cycles);
    fmt_num(used, sizeof used, times * cycles);
    fmt_num(freq, sizeof freq, hz);
    fmt_percent(pct, sizeof pct, share_ratio(hz, times, cycles));
    fprintf(out, "每秒 %s 次 × 每次 %s 个时钟周期 = %s 个时钟周期\n", ntimes, ncycles, used);
    fprintf(out, "CPU 每秒提供 %s 个时钟周期\n", freq);
    fprintf(out, "占比 = %s / %s = %s\n", used, freq, pct);
}

static void print_task(const char *period, const char *prep, const char *total, const char *cpu,
                       const char *extra);

static void print_rate_line(FILE *out, double bps) {
    char rate[160];
    fmt_rate(rate, sizeof rate, bps);
    fprintf(out, "传输率 = %s（这里的 M、G 是 10^6、10^9）\n", rate);
}

static int run_poll(void) {
    double freq = 0;
    double hz = 0;
    double cycles = 0;
    double times = 0;
    char unit[16];
    char pct[64];
    prompt("请输入「主频 单位 每次时钟周期 每秒次数」，例如 50 MHz 500 200：");
    if (scanf("%lf %15s %lf %lf", &freq, unit, &cycles, &times) != 4) {
        fprintf(stderr, "poll 需要：主频 单位 每次时钟周期 每秒次数\n");
        return 1;
    }
    if (!read_hz_unit(freq, unit, &hz)) {
        return 1;
    }
    if (!(cycles > 0) || !(times > 0)) {
        fprintf(stderr, "每次时钟周期和每秒次数要是正数\n");
        return 1;
    }
    fmt_percent(pct, sizeof pct, share_ratio(hz, times, cycles));
    fprintf(stdout, "【程序查询】\n");
    fprintf(stdout, "每秒查询次数题目已经给出，不必再用传输率去除每次字节数\n");
    print_occupy(stdout, hz, times, cycles);
    fprintf(stdout, "结果：%s\n", pct);
    return 0;
}

static int run_query(void) {
    double freq = 0;
    double hz = 0;
    double cpi = 0;
    double insns = 0;
    double rate = 0;
    double bps = 0;
    double bytes = 0;
    double times;
    double cycles;
    char funit[16];
    char runit[16];
    char gap[64];
    char pct[64];
    char nbytes[64];
    char ncpi[64];
    char nins[64];
    prompt("请输入「主频 单位 CPI 指令数 传输率 单位 每次字节」，例如 500 MHz 4 10 2 MB/s 4：");
    if (scanf("%lf %15s %lf %lf %lf %15s %lf", &freq, funit, &cpi, &insns, &rate, runit, &bytes) !=
        7) {
        fprintf(stderr, "query 需要：主频 单位 CPI 指令数 传输率 单位 每次字节\n");
        return 1;
    }
    if (!read_hz_unit(freq, funit, &hz) || !read_rate_unit(rate, runit, &bps)) {
        return 1;
    }
    if (!(cpi > 0) || !(insns > 0) || !(bytes > 0)) {
        fprintf(stderr, "CPI、指令数和每次字节数要是正数\n");
        return 1;
    }
    times = times_from_rate(bps, bytes);
    cycles = insns * cpi;
    fmt_time_short(gap, sizeof gap, bytes / bps);
    fmt_percent(pct, sizeof pct, share_ratio(hz, times, cycles));
    fmt_num(nbytes, sizeof nbytes, bytes);
    fmt_num(ncpi, sizeof ncpi, cpi);
    fmt_num(nins, sizeof nins, insns);
    fprintf(stdout, "【程序查询】\n");
    print_rate_line(stdout, bps);
    fprintf(stdout, "每次 %s B，准备时间 = %s\n", nbytes, gap);
    fprintf(stdout, "查询间隔不能长于准备时间，否则下一笔数据盖掉缓冲里还没取走的数\n");
    fprintf(stdout, "每次查询 %s 条指令 × CPI %s\n", nins, ncpi);
    print_occupy(stdout, hz, times, cycles);
    fprintf(stdout, "结果：间隔 %s，占比 %s\n", gap, pct);
    return 0;
}

static int run_intr(void) {
    double freq = 0;
    double hz = 0;
    double cpi = 0;
    double insns = 0;
    double extra = 0;
    double rate = 0;
    double bps = 0;
    double bytes = 0;
    double times;
    double cycles;
    char funit[16];
    char runit[16];
    char pct[64];
    char nins[64];
    char nextra[64];
    char ncpi[64];
    char nbytes[64];
    prompt("请输入「主频 单位 CPI 服务指令 其他指令 传输率 单位 每次字节」：");
    if (scanf("%lf %15s %lf %lf %lf %lf %15s %lf", &freq, funit, &cpi, &insns, &extra, &rate, runit,
              &bytes) != 8) {
        fprintf(stderr, "intr 需要：主频 单位 CPI 服务指令 其他指令 传输率 单位 每次字节\n");
        return 1;
    }
    if (!read_hz_unit(freq, funit, &hz) || !read_rate_unit(rate, runit, &bps)) {
        return 1;
    }
    if (!(cpi > 0) || insns < 0 || extra < 0 || !(bytes > 0)) {
        fprintf(stderr, "CPI 和每次字节数要是正数，指令数不能为负\n");
        return 1;
    }
    times = times_from_rate(bps, bytes);
    cycles = (insns + extra) * cpi;
    fmt_percent(pct, sizeof pct, share_ratio(hz, times, cycles));
    fmt_num(nins, sizeof nins, insns);
    fmt_num(nextra, sizeof nextra, extra);
    fmt_num(ncpi, sizeof ncpi, cpi);
    fmt_num(nbytes, sizeof nbytes, bytes);
    fprintf(stdout, "【程序中断】\n");
    print_rate_line(stdout, bps);
    fprintf(stdout, "每次传送 %s B。中断次数 = 传输率 / 每次字节数\n", nbytes);
    fprintf(stdout, "每次中断 (%s + %s) 条指令 × CPI %s\n", nins, nextra, ncpi);
    fprintf(stdout, "服务程序和其他开销都占 CPU。其他开销按同样的 CPI 折成时钟周期\n");
    print_occupy(stdout, hz, times, cycles);
    fprintf(stdout, "结果：%s\n", pct);
    return 0;
}

static int run_fit(void) {
    double freq = 0;
    double hz = 0;
    double cycles = 0;
    double rate = 0;
    double bps = 0;
    double bytes = 0;
    double prepare;
    double service;
    double times;
    char funit[16];
    char runit[16];
    char prep_text[64];
    char svc_text[64];
    int ok;
    prompt("请输入「主频 单位 一次中断的时钟周期 传输率 单位 每次字节」：");
    if (scanf("%lf %15s %lf %lf %15s %lf", &freq, funit, &cycles, &rate, runit, &bytes) != 6) {
        fprintf(stderr, "fit 需要：主频 单位 一次中断的时钟周期 传输率 单位 每次字节\n");
        return 1;
    }
    if (!read_hz_unit(freq, funit, &hz) || !read_rate_unit(rate, runit, &bps)) {
        return 1;
    }
    if (!(cycles > 0) || !(bytes > 0)) {
        fprintf(stderr, "时钟周期和每次字节数要是正数\n");
        return 1;
    }
    service = cycles / hz;
    prepare = bytes / bps;
    times = times_from_rate(bps, bytes);
    ok = keeps_up(prepare, service);
    fmt_time_short(prep_text, sizeof prep_text, prepare);
    fmt_time_short(svc_text, sizeof svc_text, service);
    fprintf(stdout, "【中断能否跟上】\n");
    print_rate_line(stdout, bps);
    fprintf(stdout, "一次中断响应加处理 = %s\n", svc_text);
    fprintf(stdout, "设备准备一次数据 = %s\n", prep_text);
    if (ok) {
        fprintf(stdout, "准备时间不短于一次中断，缓冲来得及被取走\n");
    } else {
        fprintf(stdout, "准备时间更短，上一次中断还没做完，下一笔数据就把缓冲盖掉了\n");
    }
    fprintf(stdout, "下面的占比只说明若硬用中断会占多少 CPU，超过 100%% 就是来不及\n");
    print_occupy(stdout, hz, times, cycles);
    fprintf(stdout, "结果：%s\n", ok ? "可以" : "不能");
    return 0;
}

static int run_dma(void) {
    double freq = 0;
    double hz = 0;
    double cycles = 0;
    double rate = 0;
    double bps = 0;
    double block = 0;
    double times;
    char funit[16];
    char runit[16];
    char pct[64];
    char nblock[64];
    char ncycles[64];
    prompt("请输入「主频 单位 预处理加后处理的时钟周期 传输率 单位 块字节」：");
    if (scanf("%lf %15s %lf %lf %15s %lf", &freq, funit, &cycles, &rate, runit, &block) != 6) {
        fprintf(stderr, "dma 需要：主频 单位 时钟周期 传输率 单位 块字节\n");
        return 1;
    }
    if (!read_hz_unit(freq, funit, &hz) || !read_rate_unit(rate, runit, &bps)) {
        return 1;
    }
    if (!(cycles > 0) || !(block > 0)) {
        fprintf(stderr, "时钟周期和块字节数要是正数\n");
        return 1;
    }
    times = times_from_rate(bps, block);
    fmt_percent(pct, sizeof pct, share_ratio(hz, times, cycles));
    fmt_num(nblock, sizeof nblock, block);
    fmt_num(ncycles, sizeof ncycles, cycles);
    fprintf(stdout, "【DMA】\n");
    print_rate_line(stdout, bps);
    fprintf(stdout, "每块 %s B，DMA 次数 = 传输率 / 块大小\n", nblock);
    fprintf(stdout, "CPU 只做预处理和后处理，共 %s 个时钟周期，不要再乘 CPI\n", ncycles);
    fprintf(stdout, "无访存冲突时，块里的字节由 DMA 控制器搬，不占 CPU\n");
    print_occupy(stdout, hz, times, cycles);
    fprintf(stdout, "结果：%s\n", pct);
    return 0;
}

static int run_serial(void) {
    double start = 0;
    double data = 0;
    double parity = 0;
    double stop = 0;
    double gap = 0;
    double seconds = 0;
    double bits;
    double per_s;
    char unit[16];
    char nbits[64];
    char nrate[64];
    char gap_text[64];
    prompt("请输入「起始位 数据位 校验位 停止位 间隔 单位」，例如 1 7 1 1 0.5 ms：");
    if (scanf("%lf %lf %lf %lf %lf %15s", &start, &data, &parity, &stop, &gap, unit) != 6) {
        fprintf(stderr, "serial 需要：起始位 数据位 校验位 停止位 间隔 单位\n");
        return 1;
    }
    if (!parse_seconds(gap, unit, &seconds) || !(seconds > 0)) {
        fprintf(stderr, "间隔要用正的时间，单位是 s、ms、us、ns\n");
        return 1;
    }
    if (start < 0 || data < 0 || parity < 0 || stop < 0) {
        fprintf(stderr, "帧里的位数不能为负\n");
        return 1;
    }
    bits = start + data + parity + stop;
    per_s = 1.0 / seconds;
    fmt_num(nbits, sizeof nbits, bits);
    fmt_num(nrate, sizeof nrate, per_s);
    fmt_time_short(gap_text, sizeof gap_text, seconds);
    fprintf(stdout, "【异步串行帧】\n");
    fprintf(stdout, "起始位 + 数据位 + 校验位 + 停止位 = %s 位\n", nbits);
    fprintf(stdout, "数据位只是字符本身。起始、校验、停止都要占通信线\n");
    fprintf(stdout, "字符间隔 %s，每秒最多 %s 个\n", gap_text, nrate);
    fprintf(stdout, "结果：%s 位，%s 个/s\n", nbits, nrate);
    return 0;
}

static int run_task(void) {
    double freq = 0;
    double hz = 0;
    double cpi = 0;
    double gap = 0;
    double seconds = 0;
    double resp = 0;
    double insns = 0;
    double launch = 0;
    double count = 0;
    TaskClocks clocks;
    char funit[16];
    char gunit[16];
    char period[64];
    char prep[64];
    char total[64];
    char cpu[64];
    char extra[64];
    prompt("请输入「主频 单位 CPI 准备时间 单位 响应周期 服务指令数 第几条启动 字符数」：");
    if (scanf("%lf %15s %lf %lf %15s %lf %lf %lf %lf", &freq, funit, &cpi, &gap, gunit, &resp,
              &insns, &launch, &count) != 9) {
        fprintf(stderr, "task 的参数没读全\n");
        return 1;
    }
    if (!read_hz_unit(freq, funit, &hz)) {
        return 1;
    }
    if (!parse_seconds(gap, gunit, &seconds)) {
        fprintf(stderr, "准备时间的单位用 s、ms、us、ns\n");
        return 1;
    }
    if (!(cpi > 0) || !(seconds > 0) || resp < 0 || !(insns > 0) || launch < 0 || launch > insns ||
        !(count > 0)) {
        fprintf(stderr, "CPI、准备时间、服务指令数和字符数要是正数，启动指令不能超过服务指令数\n");
        return 1;
    }
    clocks = task_clocks(hz, seconds, cpi, resp, insns, launch, count);
    fmt_time_short(period, sizeof period, 1.0 / hz);
    fmt_num(prep, sizeof prep, seconds * hz);
    fmt_num(total, sizeof total, clocks.total_cycles);
    fmt_num(cpu, sizeof cpu, clocks.cpu_cycles);
    fmt_num(extra, sizeof extra, clocks.extra_last);
    print_task(period, prep, total, cpu, extra);
    return 0;
}

static void print_task(const char *period, const char *prep, const char *total, const char *cpu,
                       const char *extra) {
    fprintf(stdout, "【中断方式的任务时间】\n");
    fprintf(stdout, "时钟周期 = %s，设备准备一个字符 = %s 个时钟周期\n", period, prep);
    fprintf(stdout, "一个字符的任务时间 = 准备 + 中断响应 + 启动设备前的指令 × CPI\n");
    fprintf(stdout, "启动之后的指令和下一个字符的准备重叠，不计入任务时间\n");
    fprintf(stdout, "全部字符的任务时间 = %s 个时钟周期\n", total);
    fprintf(stdout, "CPU 占用 = 字符数 × (中断响应 + 全部服务指令 × CPI) = %s 个时钟周期\n", cpu);
    fprintf(stdout, "等设备准备的那些周期里 CPU 可以跑别的程序，不计入 CPU 占用\n");
    fprintf(stdout, "最后一个字符启动后剩下的指令若单独加上，任务时间还要再加 %s 个时钟周期\n",
            extra);
    fprintf(stdout, "标准解答不要求加这最后一截\n");
    fprintf(stdout, "结果：任务 %s 个时钟周期，CPU %s 个时钟周期\n", total, cpu);
}

static int run_steal(void) {
    long long nbytes = 0;
    long long buf_bits = 0;
    long long times;
    prompt("请输入「字节数 缓冲区位数」，例如 512 64：");
    if (scanf("%lld %lld", &nbytes, &buf_bits) != 2) {
        fprintf(stderr, "steal 需要：字节数 缓冲区位数\n");
        return 1;
    }
    times = dma_requests(nbytes, buf_bits);
    if (times < 0) {
        fprintf(stderr, "字节数和缓冲区位数要是正数，而且字节数换成位之后要能整除\n");
        return 1;
    }
    fprintf(stdout, "【DMA 总线请求】\n");
    fprintf(stdout, "%lld B = %lld 位，缓冲区 %lld 位\n", nbytes, nbytes * 8, buf_bits);
    fprintf(stdout, "传送次数 = %lld / %lld = %lld\n", nbytes * 8, buf_bits, times);
    fprintf(stdout, "每传一次，DMA 控制器向 CPU 发一次总线请求\n");
    fprintf(stdout, "周期挪用时两边同时要访存，DMA 优先，否则这一块数据会丢\n");
    fprintf(stdout, "结果：%lld 次\n", times);
    return 0;
}

int io_command(const char *cmd) {
    if (strcmp(cmd, "poll") == 0) {
        return run_poll();
    }
    if (strcmp(cmd, "query") == 0) {
        return run_query();
    }
    if (strcmp(cmd, "intr") == 0) {
        return run_intr();
    }
    if (strcmp(cmd, "fit") == 0) {
        return run_fit();
    }
    if (strcmp(cmd, "dma") == 0) {
        return run_dma();
    }
    if (strcmp(cmd, "serial") == 0) {
        return run_serial();
    }
    if (strcmp(cmd, "task") == 0) {
        return run_task();
    }
    if (strcmp(cmd, "steal") == 0) {
        return run_steal();
    }
    return -1;
}
