/*
 * 电路交换、报文交换、分组交换的总时延。
 * 分组交换按存储转发逐跳模拟；各组等长时和流水线公式对账。
 */
#include <stdio.h>
#include <string.h>

#include "exchange.h"
#include "prompt.h"
#include "qty.h"

static int read_prop(long long bps, const char *mode, double value, const char *unit,
                     long long *prop, int *from_bdp, long long *bdp) {
    *from_bdp = 0;
    *bdp = 0;
    if (strcmp(mode, "time") == 0) {
        if (!parse_ns(value, unit, prop)) {
            fprintf(stderr, "传播时延的单位无法识别\n");
            return 1;
        }
        return 0;
    }
    if (strcmp(mode, "bdp") == 0) {
        if (!parse_bits(value, unit, bdp) || !qty_div_ns(*bdp, bps, prop)) {
            fprintf(stderr, "时延带宽积要能除以带宽，得到整数纳秒\n");
            return 1;
        }
        *from_bdp = 1;
        return 0;
    }
    fprintf(stderr, "传播一项只能写 time 或 bdp\n");
    return 1;
}

static int run_pipe(void) {
    int hops = 0;
    double rv, pv, pktv, hv, fv;
    char ru[16], mode[16], pu[16], pktu[16], hu[16], fu[16];
    long long bps, prop, packet, header, file, bdp;
    int from_bdp = 0;
    ExchangeIn in;
    ExchangeOut out;

    prompt("请输入「段数 带宽 单位 time或bdp 数值 单位 分组 单位 首部 单位 文件 单位」：");
    if (scanf("%d %lf %15s %15s %lf %15s %lf %15s %lf %15s %lf %15s",
              &hops, &rv, ru, mode, &pv, pu, &pktv, pktu, &hv, hu, &fv, fu) != 12) {
        fprintf(stderr, "pipe 的字段没读全\n");
        return 1;
    }
    if (!parse_bps(rv, ru, &bps) || !parse_bits(pktv, pktu, &packet) ||
        !parse_bits(hv, hu, &header) || !parse_bits(fv, fu, &file)) {
        fprintf(stderr, "pipe 的单位无法识别\n");
        return 1;
    }
    if (read_prop(bps, mode, pv, pu, &prop, &from_bdp, &bdp) != 0) {
        return 1;
    }
    in.hops = hops;
    in.bps = bps;
    in.prop_ns = prop;
    in.file_bits = file;
    in.packet_bits = packet;
    in.header_bits = header;
    if (!exchange_compute(&in, 0, &out)) {
        fprintf(stderr, "pipe 算不出来：段数 1 到 %d，分组数不超过 1000000\n",
                EXCHANGE_MAX_HOPS);
        return 1;
    }
    exchange_report_pipe(stdout, &in, &out, from_bdp, bdp);
    return 0;
}

static int run_compare(void) {
    int hops = 0;
    double rv, pv, sv, fv, pktv, hv;
    char ru[16], pu[16], su[16], fu[16], pktu[16], hu[16];
    long long bps, prop, setup, file, packet, header;
    ExchangeIn in;
    ExchangeOut out;

    prompt("请输入「段数 带宽 单位 传播 单位 建立 单位 文件 单位 分组 单位 首部 单位」：");
    if (scanf("%d %lf %15s %lf %15s %lf %15s %lf %15s %lf %15s %lf %15s",
              &hops, &rv, ru, &pv, pu, &sv, su, &fv, fu, &pktv, pktu, &hv, hu) != 13) {
        fprintf(stderr, "compare 的字段没读全\n");
        return 1;
    }
    if (!parse_bps(rv, ru, &bps) || !parse_ns(pv, pu, &prop) ||
        !parse_ns(sv, su, &setup) || !parse_bits(fv, fu, &file) ||
        !parse_bits(pktv, pktu, &packet) || !parse_bits(hv, hu, &header)) {
        fprintf(stderr, "compare 的单位无法识别\n");
        return 1;
    }
    in.hops = hops;
    in.bps = bps;
    in.prop_ns = prop;
    in.file_bits = file;
    in.packet_bits = packet;
    in.header_bits = header;
    if (!exchange_compute(&in, setup, &out)) {
        fprintf(stderr, "compare 算不出来\n");
        return 1;
    }
    exchange_report_compare(stdout, &in, setup, &out);
    return 0;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "pipe") == 0) {
        return run_pipe();
    }
    if (strcmp(cmd, "compare") == 0) {
        return run_compare();
    }
    fprintf(stderr, "未知命令：%s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[32];
    int first = 1;

    while (1) {
        prompt("请输入命令（pipe / compare），Ctrl+D 结束：");
        if (scanf("%31s", cmd) != 1) {
            break;
        }
        if (!first) {
            fputs("\n", stdout);
        }
        first = 0;
        if (dispatch(cmd) != 0) {
            return 1;
        }
    }
    return 0;
}
