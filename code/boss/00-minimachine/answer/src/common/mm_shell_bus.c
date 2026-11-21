#include "common/mm_shell_stage.h"

#include "dev/dev_arb.h"
#include "dev/dev_buf.h"
#include "dev/dev_bus.h"
#include "dev/dev_fmt.h"
#include "dev/dev_share.h"
#include "dev/dev_wire.h"

#include <stdio.h>
#include <string.h>

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int cmd_bw(const char *arg, FILE *out) {
    double freq, clocks, beats, bits, duplex, channels, hz;
    char unit[16];
    DevBw bw;
    if (sscanf(arg, "%lf %15s %lf %lf %lf %lf %lf", &freq, unit, &clocks, &beats, &bits, &duplex,
               &channels) != 7 ||
        !dev_parse_hz(freq, unit, &hz) ||
        dev_bw(hz, clocks, beats, bits, duplex, channels, &bw) != 0) {
        fprintf(out, "错误：bw 频率 单位 时钟数 传送次数 位宽 双工 通道\n");
        return -1;
    }
    fprintf(out, "bw brief=%s\n", bw.brief);
    return 1;
}

static int read_txn(const char *arg, int *each, double *hz, double *times, double *addr, double *wait,
                    double *data, double *prep, double *bits, double *payload) {
    char mode[16];
    char funit[16];
    char punit[16];
    double freq;
    double prep_raw;
    int n;
    if (sscanf(arg, "%15s", mode) != 1) {
        return -1;
    }
    *each = strcmp(mode, "each") == 0;
    if (!*each && strcmp(mode, "burst") != 0) {
        return -1;
    }
    arg = rest(arg, mode);
    if (*each) {
        n = sscanf(arg, "%lf %15s %lf %lf %lf %lf %lf %15s %lf %lf", &freq, funit, times, addr, wait, data,
                   &prep_raw, punit, bits, payload);
        if (n != 10) {
            return -1;
        }
    } else {
        n = sscanf(arg, "%lf %15s %lf %lf %lf %lf %15s %lf %lf", &freq, funit, addr, wait, data, &prep_raw,
                   punit, bits, payload);
        if (n != 9) {
            return -1;
        }
        *times = 1;
    }
    if (!dev_parse_hz(freq, funit, hz) || !dev_parse_seconds(prep_raw, punit, prep)) {
        return -1;
    }
    return 0;
}

static int cmd_txn(const char *arg, FILE *out) {
    int each = 0;
    double hz, times, addr, wait, data, prep, bits, payload;
    DevTxn txn;
    if (read_txn(arg, &each, &hz, &times, &addr, &wait, &data, &prep, &bits, &payload) != 0 ||
        dev_txn(each, hz, times, addr, wait, data, prep, bits, payload, &txn) != 0) {
        fprintf(out, "错误：txn burst|each 的参数没读全\n");
        return -1;
    }
    fprintf(out, "txn mode=%s time=%s avg=%s peak=%s\n", each ? "each" : "burst", txn.time_brief,
            txn.avg_brief, txn.peak_brief);
    return 1;
}

static int cmd_wire(const char *arg, FILE *out) {
    char kind[16];
    char lines[8][128];
    char result[32];
    int n;
    int i;
    if (sscanf(arg, "%15s", kind) != 1) {
        fprintf(out, "错误：wire sync|full|half|loose\n");
        return -1;
    }
    n = dev_wire(kind, lines, 8, result, (int)sizeof result);
    if (n < 0) {
        fprintf(out, "错误：wire sync|full|half|loose\n");
        return -1;
    }
    for (i = 0; i < n; i++) {
        fprintf(out, "%s\n", lines[i]);
    }
    fprintf(out, "wire result=%s\n", result);
    return 1;
}

static int cmd_daisy(const char *arg, FILE *out) {
    int req[16];
    int n = 0;
    int winner = -1;
    int passed = 0;
    int i;
    const char *p = arg;
    while (*p != '\0' && n < 16) {
        int bit = 0;
        int used = 0;
        if (sscanf(p, "%d%n", &bit, &used) != 1) {
            break;
        }
        req[n++] = bit;
        p += used;
    }
    if (n < 1 || dev_daisy(req, n, &winner, &passed) != 0) {
        fprintf(out, "错误：daisy 后面是 0 或 1\n");
        return -1;
    }
    for (i = 0; i < passed; i++) {
        fprintf(out, "daisy skip=%d\n", i);
    }
    if (winner < 0) {
        fprintf(out, "daisy grant=none passed=%d\n", passed);
    } else {
        fprintf(out, "daisy grant=%d passed=%d\n", winner, passed);
    }
    return 1;
}

static int cmd_share(const char *arg, FILE *out) {
    char kind[16];
    char funit[16];
    char runit[16];
    double freq, a, b, c, d, e, hz, bps;
    DevShare share;
    int rc = -1;
    memset(&share, 0, sizeof share);
    if (sscanf(arg, "%15s", kind) != 1) {
        fprintf(out, "错误：share poll|query|intr|fit|dma\n");
        return -1;
    }
    arg = rest(arg, kind);
    if (strcmp(kind, "poll") == 0 && sscanf(arg, "%lf %15s %lf %lf", &freq, funit, &a, &b) == 4 &&
        dev_parse_hz(freq, funit, &hz)) {
        rc = dev_share_poll(hz, a, b, &share);
    } else if (strcmp(kind, "query") == 0 &&
               sscanf(arg, "%lf %15s %lf %lf %lf %15s %lf", &freq, funit, &a, &b, &c, runit, &d) == 7 &&
               dev_parse_hz(freq, funit, &hz) && dev_parse_rate(c, runit, &bps)) {
        rc = dev_share_query(hz, a, b, bps, d, &share);
    } else if (strcmp(kind, "intr") == 0 &&
               sscanf(arg, "%lf %15s %lf %lf %lf %lf %15s %lf", &freq, funit, &a, &b, &c, &d, runit, &e) ==
                   8 &&
               dev_parse_hz(freq, funit, &hz) && dev_parse_rate(d, runit, &bps)) {
        rc = dev_share_intr(hz, a, b, c, bps, e, &share);
    } else if (strcmp(kind, "fit") == 0 &&
               sscanf(arg, "%lf %15s %lf %lf %15s %lf", &freq, funit, &a, &b, runit, &c) == 6 &&
               dev_parse_hz(freq, funit, &hz) && dev_parse_rate(b, runit, &bps)) {
        rc = dev_share_fit(hz, a, bps, c, &share);
    } else if (strcmp(kind, "dma") == 0 &&
               sscanf(arg, "%lf %15s %lf %lf %15s %lf", &freq, funit, &a, &b, runit, &c) == 6 &&
               dev_parse_hz(freq, funit, &hz) && dev_parse_rate(b, runit, &bps)) {
        rc = dev_share_dma(hz, a, bps, c, &share);
    }
    if (rc != 0) {
        fprintf(out, "错误：share 的参数没读全\n");
        return -1;
    }
    if (strcmp(kind, "fit") == 0) {
        fprintf(out, "share kind=fit result=%s pct=%s\n", share.keeps ? "可以" : "不能", share.pct);
    } else if (share.gap[0] != '\0') {
        fprintf(out, "share kind=%s pct=%s gap=%s\n", kind, share.pct, share.gap);
    } else {
        fprintf(out, "share kind=%s pct=%s\n", kind, share.pct);
    }
    return 1;
}

static int cmd_steal(const char *arg, FILE *out) {
    long long bytes = 0;
    long long bits = 0;
    long long n;
    if (sscanf(arg, "%lld %lld", &bytes, &bits) != 2) {
        fprintf(out, "错误：steal 字节数 缓冲区位数\n");
        return -1;
    }
    n = dev_dma_requests(bytes, bits);
    if (n < 0) {
        fprintf(out, "错误：steal 字节数 缓冲区位数\n");
        return -1;
    }
    fprintf(out, "steal requests=%lld\n", n);
    return 1;
}

static int cmd_serial(const char *arg, FILE *out) {
    double start, data, parity, stop, gap, seconds;
    char unit[16];
    DevFrame frame;
    if (sscanf(arg, "%lf %lf %lf %lf %lf %15s", &start, &data, &parity, &stop, &gap, unit) != 6 ||
        !dev_parse_seconds(gap, unit, &seconds) ||
        dev_serial(start, data, parity, stop, seconds, &frame) != 0) {
        fprintf(out, "错误：serial 起始 数据 校验 停止 间隔 单位\n");
        return -1;
    }
    fprintf(out, "serial bits=%s per_s=%s\n", frame.bits_text, frame.rate_text);
    return 1;
}

static int cmd_task(const char *arg, FILE *out) {
    double freq, cpi, gap, resp, insns, launch, count, hz, seconds;
    char funit[16];
    char gunit[16];
    DevTask task;
    if (sscanf(arg, "%lf %15s %lf %lf %15s %lf %lf %lf %lf", &freq, funit, &cpi, &gap, gunit, &resp, &insns,
               &launch, &count) != 9 ||
        !dev_parse_hz(freq, funit, &hz) || !dev_parse_seconds(gap, gunit, &seconds) ||
        dev_task(hz, seconds, cpi, resp, insns, launch, count, &task) != 0) {
        fprintf(out, "错误：task 主频 单位 CPI 准备 单位 响应周期 服务指令 启动指令 字符数\n");
        return -1;
    }
    fprintf(out, "task total=%s cpu=%s\n", task.total_text, task.cpu_text);
    return 1;
}

static void print_buf(FILE *out, int dual, int trace, int n, long long t, long long m, long long c) {
    DevBufStep step[16];
    long long sim = dev_buf_finish(dual, n, t, m, c, trace ? step : NULL);
    long long closed = dev_buf_closed(dual, n, t, m, c);
    long long avg = dev_buf_closed(dual, 2, t, m, c) - dev_buf_closed(dual, 1, t, m, c);
    int i;
    fprintf(out, "%s avg=%lld total=%lld sim=%lld\n", dual ? "double" : "single", avg, closed, sim);
    if (!trace) {
        return;
    }
    for (i = 0; i < n; i++) {
        fprintf(out, "block=%d buf=%d in=%lld-%lld move=%lld-%lld cpu=%lld-%lld\n", step[i].block, step[i].buf,
                step[i].in0, step[i].in1, step[i].mv0, step[i].mv1, step[i].cpu0, step[i].cpu1);
    }
}

static int cmd_buf(const char *arg, FILE *out) {
    char kind[16];
    int trace = 0;
    int n = 0;
    long long t = 0;
    long long m = 0;
    long long c = 0;
    int which;
    if (starts(arg, "trace")) {
        trace = 1;
        arg = rest(arg, "trace");
    }
    if (sscanf(arg, "%15s %d %lld %lld %lld", kind, &n, &t, &m, &c) != 5 || n < 1 || (trace && n > 16)) {
        fprintf(out, "错误：buf [trace] single|double|both 块数 T M C\n");
        return -1;
    }
    which = strcmp(kind, "both") == 0 ? 2 : strcmp(kind, "double") == 0 ? 1 : strcmp(kind, "single") == 0 ? 0 : -1;
    if (which < 0) {
        fprintf(out, "错误：buf [trace] single|double|both 块数 T M C\n");
        return -1;
    }
    if (which != 1) {
        print_buf(out, 0, trace, n, t, m, c);
    }
    if (which != 0) {
        print_buf(out, 1, trace, n, t, m, c);
    }
    return 1;
}

int mm_shell_bus(char *line, FILE *out) {
    if (starts(line, "bw")) {
        return cmd_bw(rest(line, "bw"), out);
    }
    if (starts(line, "txn")) {
        return cmd_txn(rest(line, "txn"), out);
    }
    if (starts(line, "wire")) {
        return cmd_wire(rest(line, "wire"), out);
    }
    if (starts(line, "daisy")) {
        return cmd_daisy(rest(line, "daisy"), out);
    }
    if (starts(line, "share")) {
        return cmd_share(rest(line, "share"), out);
    }
    if (starts(line, "steal")) {
        return cmd_steal(rest(line, "steal"), out);
    }
    if (starts(line, "serial")) {
        return cmd_serial(rest(line, "serial"), out);
    }
    if (starts(line, "task")) {
        return cmd_task(rest(line, "task"), out);
    }
    if (starts(line, "buf")) {
        return cmd_buf(rest(line, "buf"), out);
    }
    return 0;
}
