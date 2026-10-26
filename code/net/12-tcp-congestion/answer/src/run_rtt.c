#include "run_rtt.h"

#include "line.h"
#include "rtt.h"
#include "units.h"

#include <stdio.h>
#include <string.h>

static RttEst g;

static void sec(long us, char *out, size_t cap) {
    fmt_seconds(us, out, cap);
}

static void show_ignored(const RttStep *s) {
    char sample[64], rtts[64], rttd[64], rto[64];

    sec(s->sample_us, sample, sizeof sample);
    sec(s->rtts_us, rtts, sizeof rtts);
    sec(s->rttd_us, rttd, sizeof rttd);
    sec(s->rto_us, rto, sizeof rto);
    printf("【重传后的样本】\n");
    printf("样本 RTT = %s。这段被重传过，分不清确认的是哪一次。\n", sample);
    printf("Karn 算法：这次不更新 RTTs 和 RTTD。RTTs 仍是 %s，RTTD 仍是 %s。\n", rtts, rttd);
    printf("超时重传时间保持翻倍后的 %s。\n", rto);
    printf("结果：%s\n", rto);
}

static void show_first(const RttStep *s, long prev_initial) {
    char sample[64], rtts[64], rttd[64], rto[64], prev[64];
    long four = 4 * s->rttd_us;

    sec(s->sample_us, sample, sizeof sample);
    sec(s->rtts_us, rtts, sizeof rtts);
    sec(s->rttd_us, rttd, sizeof rttd);
    sec(s->rto_us, rto, sizeof rto);
    sec(four, prev, sizeof prev);
    printf("【第 1 个 RTT 样本】\n");
    printf("样本 RTT = %s。\n", sample);
    printf("第一次测量：RTTs = %s，RTTD = %s / 2 = %s。\n", rtts, sample, rttd);
    printf("RTO = RTTs + 4 × RTTD = %s + %s = %s。\n", rtts, prev, rto);
    if (prev_initial > 0) {
        sec(prev_initial, prev, sizeof prev);
        printf("连接开始时先放的 %s 被这次测量换掉，不拿去做加权。\n", prev);
    }
    printf("结果：%s\n", rto);
}

static void show_next(const RttStep *s) {
    char sample[64], old_rtts[64], old_rttd[64], rtts[64], rttd[64], rto[64], rfc[64];
    char part_a[64], part_b[64], gap[64], part_c[64], part_d[64], four[64];

    sec(s->sample_us, sample, sizeof sample);
    sec(s->old_rtts_us, old_rtts, sizeof old_rtts);
    sec(s->old_rttd_us, old_rttd, sizeof old_rttd);
    sec(s->rtts_us, rtts, sizeof rtts);
    sec(s->rttd_us, rttd, sizeof rttd);
    sec(s->rto_us, rto, sizeof rto);
    sec(s->rfc_rto_us, rfc, sizeof rfc);
    sec((long)((7LL * s->old_rtts_us) / 8), part_a, sizeof part_a);
    sec(s->sample_us / 8, part_b, sizeof part_b);
    sec(s->rtts_us > s->sample_us ? s->rtts_us - s->sample_us : s->sample_us - s->rtts_us,
        gap, sizeof gap);
    sec((long)((3LL * s->old_rttd_us) / 4), part_c, sizeof part_c);
    sec((s->sample_us > s->rtts_us ? s->sample_us - s->rtts_us : s->rtts_us - s->sample_us) / 4,
        part_d, sizeof part_d);
    sec(4 * s->rttd_us, four, sizeof four);
    printf("【后续 RTT 样本】\n");
    printf("样本 RTT = %s。α = 1/8，β = 1/4。旧 RTTs = %s，旧 RTTD = %s。\n",
           sample, old_rtts, old_rttd);
    printf("新 RTTs = (7/8) × %s + (1/8) × %s = %s + %s = %s。\n",
           old_rtts, sample, part_a, part_b, rtts);
    printf("|新 RTTs - 样本| = %s。\n", gap);
    printf("新 RTTD = (3/4) × %s + (1/4) × %s = %s + %s = %s。\n",
           old_rttd, gap, part_c, part_d, rttd);
    printf("RTO = %s + %s = %s。\n", rtts, four, rto);
    printf("偏差若用更新前的 RTTs，RTO 是 %s。王道先更新 RTTs，本关取 %s。\n", rfc, rto);
    printf("结果：%s\n", rto);
}

static void show_step(const RttStep *s, long prev_initial) {
    output_begin();
    if (s->ignored) {
        show_ignored(s);
    } else if (s->first) {
        show_first(s, prev_initial);
    } else {
        show_next(s);
    }
}

static int cmd_init(char *p) {
    char tok[64];
    long us;
    char text[64];

    if (take_need(&p, tok, (int)sizeof tok, "缺少初始 RTO") != 0 || !rest_blank(p)) {
        return rest_blank(p) ? -1 : reject("这一行多了内容");
    }
    if (parse_time_us(tok, &us) != 0) {
        return reject("时间要带 s 或 ms");
    }
    if (rtt_set_rto(&g, us) != NULL) {
        return reject(rtt_set_rto(&g, us));
    }
    output_begin();
    fmt_seconds(us, text, sizeof text);
    printf("【初始超时】\n");
    printf("还没有 RTT 样本，超时重传时间先取 %s。\n", text);
    printf("结果：%s\n", text);
    return 0;
}

static int cmd_sample(char *p) {
    char tok[64];
    long us;
    long prev;
    RttStep step;

    if (take_need(&p, tok, (int)sizeof tok, "缺少 RTT 样本") != 0 || !rest_blank(p)) {
        return rest_blank(p) ? -1 : reject("这一行多了内容");
    }
    if (parse_time_us(tok, &us) != 0) {
        return reject("时间要带 s 或 ms");
    }
    prev = g.have ? 0 : g.rto_us;
    if (rtt_sample(&g, us, &step) != NULL) {
        return reject("RTT 样本要是正数");
    }
    show_step(&step, prev);
    return 0;
}

static int cmd_rexmit(char *p) {
    long rto = 0;
    char text[64];
    const char *err;

    if (!rest_blank(p)) {
        return reject("这一行多了内容");
    }
    err = rtt_rexmit(&g, &rto);
    if (err != NULL) {
        return reject(err);
    }
    output_begin();
    fmt_seconds(rto, text, sizeof text);
    printf("【超时重传】\n");
    printf("报文段重传一次，超时重传时间翻倍，变成 %s。\n", text);
    printf("下一次样本来自这个重传过的段，不拿来更新 RTTs。\n");
    printf("结果：%s\n", text);
    return 0;
}

int run_rtt(char *line) {
    char cmd[32];
    char *p = line;

    if (take_need(&p, cmd, (int)sizeof cmd, "缺少命令") != 0) {
        return -1;
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(p);
    }
    if (strcmp(cmd, "sample") == 0) {
        return cmd_sample(p);
    }
    if (strcmp(cmd, "rexmit") == 0) {
        return cmd_rexmit(p);
    }
    return reject("不认识的命令");
}
