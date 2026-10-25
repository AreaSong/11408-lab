/*
 * 时延、时延带宽积、停等利用率和吞吐量。
 * 分组经过多段链路的总时延在 switching.c，数据报和虚电路在 path.c。
 */
#include <stdio.h>
#include <string.h>

#include "delay.h"
#include "prompt.h"
#include "qty.h"

#define BOTTLENECK_MAX 8

static int run_link(void) {
    double lv, rv, dv, sv, pv, qv;
    char lu[16], ru[16], du[16], su[16], pu[16], qu[16];
    long long bits, bps, meters, mps, proc, queue;
    LinkOut out;

    prompt("请输入「长度 单位 带宽 单位 距离 单位 速率 单位 处理 单位 排队 单位」：");
    if (scanf("%lf %15s %lf %15s %lf %15s %lf %15s %lf %15s %lf %15s",
              &lv, lu, &rv, ru, &dv, du, &sv, su, &pv, pu, &qv, qu) != 12) {
        fprintf(stderr, "link 需要长度、带宽、距离、传播速率、处理时延、排队时延，各带单位\n");
        return 1;
    }
    if (!parse_bits(lv, lu, &bits) || !parse_bps(rv, ru, &bps) ||
        !parse_meters(dv, du, &meters) || !parse_mps(sv, su, &mps) ||
        !parse_ns(pv, pu, &proc) || !parse_ns(qv, qu, &queue)) {
        fprintf(stderr, "link 的单位无法识别\n");
        return 1;
    }
    if (!link_compute(bits, bps, meters, mps, proc, queue, &out)) {
        fprintf(stderr, "link 算不出来：发送时延、传播时延、时延带宽积都要落在整数上\n");
        return 1;
    }
    link_report(stdout, &out);
    return 0;
}

static int run_util(void) {
    double dv, av, rv, pv;
    char du[16], au[16], ru[16], pu[16];
    long long data, ack, bps, prop;
    UtilOut out;

    prompt("请输入「数据帧 单位 确认帧 单位 带宽 单位 单向传播 单位」：");
    if (scanf("%lf %15s %lf %15s %lf %15s %lf %15s",
              &dv, du, &av, au, &rv, ru, &pv, pu) != 8) {
        fprintf(stderr, "util 需要数据帧、确认帧、带宽、单向传播时延，各带单位\n");
        return 1;
    }
    if (!parse_bits(dv, du, &data) || !parse_bits(av, au, &ack) ||
        !parse_bps(rv, ru, &bps) || !parse_ns(pv, pu, &prop)) {
        fprintf(stderr, "util 的单位无法识别\n");
        return 1;
    }
    if (!util_compute(data, ack, bps, prop, &out)) {
        fprintf(stderr, "util 算不出来：数据帧要为正，确认帧可以为 0\n");
        return 1;
    }
    util_report(stdout, &out);
    return 0;
}

static int run_bottleneck(void) {
    long long bps[BOTTLENECK_MAX];
    long long best = 0;
    int n = 0;
    int i;

    prompt("请输入链路段数：");
    if (scanf("%d", &n) != 1 || n < 1 || n > BOTTLENECK_MAX) {
        fprintf(stderr, "bottleneck 的段数要在 1 到 %d 之间\n", BOTTLENECK_MAX);
        return 1;
    }
    for (i = 0; i < n; i++) {
        double value = 0;
        char unit[16];
        prompt("请输入第 %d 段的「速率 单位」：", i + 1);
        if (scanf("%lf %15s", &value, unit) != 2 || !parse_bps(value, unit, &bps[i])) {
            fprintf(stderr, "第 %d 段的速率无法识别\n", i + 1);
            return 1;
        }
    }
    if (!bottleneck_bps(bps, n, &best)) {
        fprintf(stderr, "bottleneck 算不出来\n");
        return 1;
    }
    bottleneck_report(stdout, bps, n, best);
    return 0;
}

static int run_goodput(void) {
    double pv, hv, rv;
    char pu[16], hu[16], ru[16];
    long long payload, header, bps, result;

    prompt("请输入「数据 单位 首部 单位 带宽 单位」：");
    if (scanf("%lf %15s %lf %15s %lf %15s", &pv, pu, &hv, hu, &rv, ru) != 6) {
        fprintf(stderr, "goodput 需要数据、首部、带宽，各带单位\n");
        return 1;
    }
    if (!parse_bits(pv, pu, &payload) || !parse_bits(hv, hu, &header) ||
        !parse_bps(rv, ru, &bps)) {
        fprintf(stderr, "goodput 的单位无法识别\n");
        return 1;
    }
    if (!goodput_bps(payload, header, bps, &result)) {
        fprintf(stderr, "goodput 算不出来：有效吞吐要落在整数 bps 上\n");
        return 1;
    }
    goodput_report(stdout, payload, header, bps, result);
    return 0;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "link") == 0) {
        return run_link();
    }
    if (strcmp(cmd, "util") == 0) {
        return run_util();
    }
    if (strcmp(cmd, "bottleneck") == 0) {
        return run_bottleneck();
    }
    if (strcmp(cmd, "goodput") == 0) {
        return run_goodput();
    }
    fprintf(stderr, "未知命令：%s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[32];
    int first = 1;

    while (1) {
        prompt("请输入命令（link / util / bottleneck / goodput），Ctrl+D 结束：");
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
