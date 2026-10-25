#include "console.h"

#include "capacity.h"
#include "report.h"
#include "unit.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int tail_ok(const char *line, int used) {
    if (used < 0) {
        return 0;
    }
    while (line[used] == ' ' || line[used] == '\t' || line[used] == '\r' || line[used] == '\n') {
        used++;
    }
    return line[used] == '\0';
}

static int from_bits_of(const char *mode, int *from_bits) {
    if (strcmp(mode, "bits") == 0) {
        *from_bits = 1;
        return 1;
    }
    if (strcmp(mode, "baud") == 0) {
        *from_bits = 0;
        return 1;
    }
    return 0;
}

static int rate_of(int from_bits, double value, const char *unit, long long *rate) {
    if (from_bits) {
        return parse_bps(value, unit, rate);
    }
    return parse_baud(value, unit, rate);
}

static int snr_of(double value, const char *unit, int *use_db, int *db,
                  unsigned long long *ratio) {
    long long count;

    if (!parse_count(value, &count) || count < 0) {
        return 0;
    }
    if (strcmp(unit, "dB") == 0) {
        *use_db = 1;
        *db = (int)count;
        *ratio = 0;
        return 1;
    }
    if (strcmp(unit, "ratio") == 0) {
        *use_db = 0;
        *db = -1;
        *ratio = (unsigned long long)count;
        return 1;
    }
    return 0;
}

static int convert_states(const char *mode, double value, const char *unit, long long states) {
    int from_bits = 0;
    long long rate = 0;
    Symbol sym;
    ConvertOut out;

    if (!from_bits_of(mode, &from_bits)) {
        return fail("convert 的第一项只能是 bits 或 baud");
    }
    if (!rate_of(from_bits, value, unit, &rate)) {
        return fail("速率或波特率的单位无法识别");
    }
    if (!symbol_from_states(states, &sym)) {
        return fail("状态数必须是 2 的幂，并且至少为 2");
    }
    if (!convert_rate(from_bits, rate, &sym, &out)) {
        return fail("比特率和每码元比特数要能整除");
    }
    report_convert(stdout, &out);
    return 0;
}

static int convert_phase(const char *mode, double value, const char *unit, int phases, int amps) {
    int from_bits = 0;
    long long rate = 0;
    Symbol sym;
    ConvertOut out;

    if (!from_bits_of(mode, &from_bits)) {
        return fail("convert 的第一项只能是 bits 或 baud");
    }
    if (!rate_of(from_bits, value, unit, &rate)) {
        return fail("速率或波特率的单位无法识别");
    }
    if (!symbol_from_phase(phases, amps, &sym)) {
        return fail("相位和振幅的乘积必须是 2 的幂");
    }
    if (!convert_rate(from_bits, rate, &sym, &out)) {
        return fail("比特率和每码元比特数要能整除");
    }
    report_convert(stdout, &out);
    return 0;
}

static int convert_named(const char *mode, double value, const char *unit, const char *code) {
    int from_bits = 0;
    int manchester = 0;
    long long rate = 0;
    CodeOut out;

    if (strcmp(code, "manchester") == 0) {
        manchester = 1;
    } else if (strcmp(code, "nrz") != 0) {
        return fail("code 只能是 manchester 或 nrz");
    }
    if (!from_bits_of(mode, &from_bits)) {
        return fail("convert 的第一项只能是 bits 或 baud");
    }
    if (!rate_of(from_bits, value, unit, &rate)) {
        return fail("速率或波特率的单位无法识别");
    }
    if (!convert_code(from_bits, rate, manchester, &out)) {
        return fail("码元速率和每个比特的码元数要能整除");
    }
    report_code(stdout, &out);
    return 0;
}

static int run_convert(const char *line) {
    char mode[16];
    char unit[16];
    char code[16];
    double value = 0;
    long long states = 0;
    int phases = 0;
    int amps = 0;
    int used = -1;
    int n;

    n = sscanf(line, " convert %15s %lf %15s states %lld %n", mode, &value, unit, &states, &used);
    if (n == 4 && tail_ok(line, used)) {
        return convert_states(mode, value, unit, states);
    }
    used = -1;
    n = sscanf(line, " convert %15s %lf %15s phase %d amp %d %n",
               mode, &value, unit, &phases, &amps, &used);
    if (n == 5 && tail_ok(line, used)) {
        return convert_phase(mode, value, unit, phases, amps);
    }
    used = -1;
    n = sscanf(line, " convert %15s %lf %15s code %15s %n", mode, &value, unit, code, &used);
    if (n == 4 && tail_ok(line, used)) {
        return convert_named(mode, value, unit, code);
    }
    return fail("convert 的格式见 TASK.md");
}

static int nyquist_forward(long long hz, const Symbol *sym) {
    NyquistOut out;

    if (!nyquist_compute(hz, sym, &out)) {
        return fail("奈奎斯特极限算不出来");
    }
    report_nyquist(stdout, &out);
    return 0;
}

static int nyquist_states(double hv, const char *hu, long long states) {
    long long hz = 0;
    Symbol sym;

    if (!parse_hz(hv, hu, &hz)) {
        return fail("带宽的单位无法识别");
    }
    if (!symbol_from_states(states, &sym)) {
        return fail("状态数必须是 2 的幂，并且至少为 2");
    }
    return nyquist_forward(hz, &sym);
}

static int nyquist_phase(double hv, const char *hu, int phases, int amps) {
    long long hz = 0;
    Symbol sym;

    if (!parse_hz(hv, hu, &hz)) {
        return fail("带宽的单位无法识别");
    }
    if (!symbol_from_phase(phases, amps, &sym)) {
        return fail("相位和振幅的乘积必须是 2 的幂");
    }
    return nyquist_forward(hz, &sym);
}

static int nyquist_rate(double hv, const char *hu, double rv, const char *ru) {
    long long hz = 0;
    long long bps = 0;
    NyquistOut out;

    if (!parse_hz(hv, hu, &hz) || !parse_bps(rv, ru, &bps)) {
        return fail("带宽或速率的单位无法识别");
    }
    if (!nyquist_solve(hz, bps, &out)) {
        return fail("速率要能被 2W 整除，每个码元的比特数还得是 1 到 30 的整数");
    }
    report_solve(stdout, &out);
    return 0;
}

static int run_nyquist(const char *line) {
    char hu[16];
    char ru[16];
    double hv = 0;
    double rv = 0;
    long long states = 0;
    int phases = 0;
    int amps = 0;
    int used = -1;
    int n;

    n = sscanf(line, " nyquist %lf %15s states %lld %n", &hv, hu, &states, &used);
    if (n == 3 && tail_ok(line, used)) {
        return nyquist_states(hv, hu, states);
    }
    used = -1;
    n = sscanf(line, " nyquist %lf %15s phase %d amp %d %n", &hv, hu, &phases, &amps, &used);
    if (n == 4 && tail_ok(line, used)) {
        return nyquist_phase(hv, hu, phases, amps);
    }
    used = -1;
    n = sscanf(line, " nyquist %lf %15s rate %lf %15s %n", &hv, hu, &rv, ru, &used);
    if (n == 4 && tail_ok(line, used)) {
        return nyquist_rate(hv, hu, rv, ru);
    }
    return fail("nyquist 的格式见 TASK.md");
}

static int run_shannon(const char *line) {
    double hv = 0;
    double sv = 0;
    char hu[16];
    char su[16];
    int percent = 100;
    int used = -1;
    int n;
    int show = 0;
    int use_db = 0;
    int db = 0;
    unsigned long long ratio = 0;
    long long hz = 0;
    ShannonOut out;

    n = sscanf(line, " shannon %lf %15s %lf %15s actual %d %n", &hv, hu, &sv, su, &percent, &used);
    if (n == 5 && tail_ok(line, used)) {
        show = 1;
    } else {
        used = -1;
        n = sscanf(line, " shannon %lf %15s %lf %15s %n", &hv, hu, &sv, su, &used);
        if (!(n == 4 && tail_ok(line, used))) {
            return fail("shannon 的格式见 TASK.md");
        }
    }
    if (!parse_hz(hv, hu, &hz)) {
        return fail("带宽的单位无法识别");
    }
    if (!snr_of(sv, su, &use_db, &db, &ratio)) {
        return fail("信噪比的单位只能是 dB 或 ratio");
    }
    if (show && (percent < 1 || percent > 100)) {
        return fail("实际百分比要在 1 到 100 之间");
    }
    if (!shannon_compute(hz, use_db, db, ratio, show, percent, &out)) {
        return fail("香农极限算不出来：分贝要是 10 的倍数；要乘百分比时 log2 还得能收成整数");
    }
    report_shannon(stdout, &out);
    return 0;
}

static int bound_states(double value, const char *unit) {
    int use_db = 0;
    int db = 0;
    unsigned long long ratio = 0;
    NeedOut out;

    if (!snr_of(value, unit, &use_db, &db, &ratio)) {
        return fail("信噪比的单位只能是 dB 或 ratio");
    }
    if (!states_need(use_db, db, ratio, &out)) {
        return fail("最少状态数算不出来：分贝要是 10 的倍数，比值要在 1 到 1000000");
    }
    report_states(stdout, &out);
    return 0;
}

static int bound_rates(double hv, const char *hu, long long states, double sv, const char *su) {
    long long hz = 0;
    int use_db = 0;
    int db = 0;
    unsigned long long ratio = 0;
    Symbol sym;
    BoundOut out;

    if (!parse_hz(hv, hu, &hz)) {
        return fail("带宽的单位无法识别");
    }
    if (!symbol_from_states(states, &sym)) {
        return fail("状态数必须是 2 的幂，并且至少为 2");
    }
    if (!snr_of(sv, su, &use_db, &db, &ratio)) {
        return fail("信噪比的单位只能是 dB 或 ratio");
    }
    if (!bound_compute(hz, &sym, use_db, db, ratio, &out)) {
        return fail("比不出较小值：香农的 log2 要能收成整数比特");
    }
    report_bound(stdout, &out);
    return 0;
}

static int run_bound(const char *line) {
    char what[16];
    char hu[16];
    char su[16];
    double hv = 0;
    double sv = 0;
    long long states = 0;
    int used = -1;
    int n;

    if (sscanf(line, " bound %15s", what) != 1) {
        return fail("bound 的格式见 TASK.md");
    }
    if (strcmp(what, "states") == 0) {
        n = sscanf(line, " bound states %lf %15s %n", &sv, su, &used);
        if (n == 2 && tail_ok(line, used)) {
            return bound_states(sv, su);
        }
        return fail("bound states 的格式见 TASK.md");
    }
    n = sscanf(line, " bound %lf %15s states %lld %lf %15s %n",
               &hv, hu, &states, &sv, su, &used);
    if (n == 5 && tail_ok(line, used)) {
        return bound_rates(hv, hu, states, sv, su);
    }
    return fail("bound 的格式见 TASK.md");
}

int run_command(const char *line) {
    char cmd[32];

    if (sscanf(line, "%31s", cmd) != 1) {
        return 0;
    }
    if (strcmp(cmd, "convert") == 0) {
        return run_convert(line);
    }
    if (strcmp(cmd, "nyquist") == 0) {
        return run_nyquist(line);
    }
    if (strcmp(cmd, "shannon") == 0) {
        return run_shannon(line);
    }
    if (strcmp(cmd, "bound") == 0) {
        return run_bound(line);
    }
    fprintf(stderr, "未知命令：%s\n", cmd);
    return 1;
}
