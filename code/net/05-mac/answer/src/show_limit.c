#include "show_limit.h"

#include "show_back.h"
#include "show_io.h"

static void print_frame(const Span *sp, long long bps, long long mps) {
    char rate[64];
    char speed[96];
    char dist[64];
    char bits[64];
    char prop[32];
    char slot[32];

    fmt_rate(rate, sizeof rate, bps);
    fmt_speed(speed, sizeof speed, mps);
    fmt_meters(dist, sizeof dist, sp->meters);
    fmt_bits(bits, sizeof bits, sp->bits);
    fmt_us(prop, sizeof prop, sp->prop_ns);
    fmt_us(slot, sizeof slot, sp->send_ns);
    printf("【最短帧】\n");
    printf("带宽 %s，距离 %s，速度 %s。\n", rate, dist, speed);
    printf("单程传播 = 距离 / 速度 = %s。\n", prop);
    if (sp->dev_ns > 0) {
        char dev[32];
        char half[32];

        fmt_us(dev, sizeof dev, sp->dev_ns);
        fmt_us(half, sizeof half, sp->half_ns);
        printf("单程还要加上设备时延 %s，单程预算 = %s。\n", dev, half);
        printf("争用期 = 2 × 单程预算 = %s。\n", slot);
    } else {
        printf("争用期 = 2 × 单程传播 = %s。\n", slot);
    }
    printf("最短帧 = 争用期 × 带宽 = %s。\n", bits);
    printf("帧的发送时间至少要盖住争用期，否则发完了才发现冲突。\n");
    printf("结果：%s\n", bits);
}

static int cmd_frame(const char *p) {
    long long bps = 0;
    long long meters = 0;
    long long mps = 0;
    long long dev = 0;
    Span sp;

    if (!need_key(&p, "bw") || !take_num(&p, parse_bps, &bps) || !need_key(&p, "dist") ||
        !take_num(&p, parse_meters, &meters) || !need_key(&p, "speed") || !take_num(&p, parse_speed, &mps) ||
        !optional_dev(&p, &dev)) {
        return err("命令格式不对");
    }
    if (!span_by_distance(bps, meters, mps, dev, &sp)) {
        return bad_span();
    }
    print_frame(&sp, bps, mps);
    return 0;
}

static void print_reach(const Span *sp, long long bps, long long mps) {
    char bits[64];
    char rate[64];
    char speed[96];
    char slot[32];
    char half[32];
    char prop[32];
    char dist[64];

    fmt_bits(bits, sizeof bits, sp->bits);
    fmt_rate(rate, sizeof rate, bps);
    fmt_speed(speed, sizeof speed, mps);
    fmt_us(slot, sizeof slot, sp->send_ns);
    fmt_us(half, sizeof half, sp->half_ns);
    fmt_us(prop, sizeof prop, sp->prop_ns);
    fmt_meters(dist, sizeof dist, sp->meters);
    printf("【最远距离】\n");
    printf("最短帧 %s，带宽 %s。按输入的帧长计算，不加前导码。\n", bits, rate);
    printf("争用期 = 帧长 / 带宽 = %s。\n", slot);
    printf("单程预算 = 争用期 / 2 = %s。往返才构成一个争用期。\n", half);
    if (sp->dev_ns > 0) {
        char dev[32];

        fmt_us(dev, sizeof dev, sp->dev_ns);
        printf("单程经过设备，处理时延 %s 从单程预算里扣掉。\n", dev);
        printf("传播时延 = %s − %s = %s。\n", half, dev, prop);
    } else {
        printf("没有设备时延，传播时延就是单程预算 %s。\n", prop);
    }
    printf("速度 %s。\n", speed);
    printf("距离 = 传播时延 × 速度 = %s。\n", dist);
    printf("结果：%s\n", dist);
}

static int cmd_reach(const char *p) {
    long long bits = 0;
    long long bps = 0;
    long long mps = 0;
    long long dev = 0;
    Span sp;

    if (!need_key(&p, "frame") || !take_num(&p, parse_bits, &bits) || !need_key(&p, "bw") ||
        !take_num(&p, parse_bps, &bps) || !need_key(&p, "speed") || !take_num(&p, parse_speed, &mps) ||
        !optional_dev(&p, &dev)) {
        return err("命令格式不对");
    }
    if (!span_by_frame(bits, bps, mps, dev, &sp)) {
        return bad_span();
    }
    print_reach(&sp, bps, mps);
    return 0;
}

static int cmd_tau(const char *p) {
    long long bits = 0;
    long long bps = 0;
    long long send_ns = 0;
    long long half_ns = 0;
    char frame[64];
    char rate[64];
    char send[32];
    char half[32];

    if (!need_key(&p, "frame") || !take_num(&p, parse_bits, &bits) || !need_key(&p, "bw") ||
        !take_num(&p, parse_bps, &bps) || !scan_blank(p)) {
        return err("命令格式不对");
    }
    if (!span_times(bits, bps, &send_ns, &half_ns)) {
        return bad_span();
    }
    fmt_bits(frame, sizeof frame, bits);
    fmt_rate(rate, sizeof rate, bps);
    fmt_us(send, sizeof send, send_ns);
    fmt_us(half, sizeof half, half_ns);
    printf("【单向传播上限】\n");
    printf("最短帧 %s，带宽 %s。\n", frame, rate);
    printf("发送时延 = 帧长 / 带宽 = %s。最短帧时，这就是争用期。\n", send);
    printf("发送时延要盖住往返传播，单向最多是一半。\n");
    printf("结果：%s\n", half);
    return 0;
}

static int cmd_delta(const char *p) {
    char dir[16];
    long long bits = 0;
    long long bps = 0;
    long long mps = 0;
    long long rtt = 0;
    long long one = 0;
    long long meters = 0;
    char frame[64];
    char rate[64];
    char speed[96];
    char rtt_s[32];
    char one_s[32];
    char dist[64];
    const char *word;

    if (scan_word(&p, dir, (int)sizeof dir) != 1 || (strcmp(dir, "less") != 0 && strcmp(dir, "more") != 0)) {
        return err("命令格式不对");
    }
    if (!take_num(&p, parse_bits, &bits) || !need_key(&p, "bw") || !take_num(&p, parse_bps, &bps) ||
        !need_key(&p, "speed") || !take_num(&p, parse_speed, &mps) || !scan_blank(p)) {
        return err("命令格式不对");
    }
    if (!span_delta(bits, bps, mps, &rtt, &one, &meters)) {
        return bad_span();
    }
    word = strcmp(dir, "less") == 0 ? "减少" : "增加";
    fmt_bits(frame, sizeof frame, bits);
    fmt_rate(rate, sizeof rate, bps);
    fmt_speed(speed, sizeof speed, mps);
    fmt_us(rtt_s, sizeof rtt_s, rtt);
    fmt_us(one_s, sizeof one_s, one);
    fmt_meters(dist, sizeof dist, meters);
    printf("【距离变化】\n");
    printf("帧长%s %s，带宽 %s，速度 %s。\n", word, frame, rate, speed);
    printf("发送时延%s %s。这是往返，单程%s %s。\n", word, rtt_s, word, one_s);
    printf("距离%s %s × 速度 = %s。\n", word, one_s, dist);
    printf("发送时间变了，往返传播的预算跟着变，最远距离也跟着变。\n");
    printf("结果：%s %s\n", word, dist);
    return 0;
}

static int cmd_detect(const char *p) {
    long long meters = 0;
    long long mps = 0;
    long long tau = 0;
    char dist[64];
    char speed[96];
    char tau_s[32];
    char back[32];

    if (!need_key(&p, "dist") || !take_num(&p, parse_meters, &meters) || !need_key(&p, "speed") ||
        !take_num(&p, parse_speed, &mps) || !scan_blank(p)) {
        return err("命令格式不对");
    }
    if (!span_prop(meters, mps, &tau)) {
        return bad_span();
    }
    fmt_meters(dist, sizeof dist, meters);
    fmt_speed(speed, sizeof speed, mps);
    fmt_us(tau_s, sizeof tau_s, tau);
    fmt_us(back, sizeof back, tau * 2);
    printf("【冲突检测时间】\n");
    printf("距离 %s，速度 %s。\n", dist, speed);
    printf("单程传播 τ = 距离 / 速度 = %s。\n", tau_s);
    printf("最短：两边同时发送，在中点相撞，碰撞信号再传回两端。两端都发现是 1 个 τ。\n");
    printf("最长：一边的信号快到对端时，对端才开始发。近端马上发现，远端要等碰撞信号传回来。\n");
    printf("两端都发现是 2τ。\n");
    printf("结果：最短 %s，最长 %s\n", tau_s, back);
    return 0;
}

static void print_goodput(const Goodput *g) {
    char frame[80];
    char ack[80];
    char data[80];
    char frame_ms[32];
    char ack_ms[32];
    char tau_s[32];
    char rtt_s[32];
    char period[32];
    char rate[64];
    char mbps[64];

    fmt_bits(frame, sizeof frame, g->frame_bits);
    fmt_bits(ack, sizeof ack, g->ack_bits);
    fmt_bits(data, sizeof data, g->data_bits);
    fmt_time_one(frame_ms, sizeof frame_ms, g->frame_ns);
    fmt_time_one(ack_ms, sizeof ack_ms, g->ack_ns);
    fmt_us(tau_s, sizeof tau_s, g->tau_ns);
    fmt_us(rtt_s, sizeof rtt_s, g->tau_ns * 2);
    fmt_time_one(period, sizeof period, g->period_ns);
    fmt_rate(rate, sizeof rate, g->out_bps);
    snprintf(mbps, sizeof mbps, "%s%d.%02d Mbps（M = 10^6）", g->exact ? "" : "约 ", g->centi / 100,
             g->centi % 100);
    printf("【有效数据速率】\n");
    printf("数据帧 %s，发送时延 %s。\n", frame, frame_ms);
    printf("确认帧 %s，发送时延 %s。\n", ack, ack_ms);
    printf("单程传播 %s，往返 %s。\n", tau_s, rtt_s);
    printf("周期 = 数据帧发送 + 确认帧发送 + 往返传播 = %s。\n", period);
    printf("有效数据 %s。分子用 data，分母含整帧和确认帧的发送时延。\n", data);
    printf("速率 = %lld bit / %s = %s，%s。\n", g->data_bits, period, rate, mbps);
    printf("结果：%s\n", mbps);
}

static int cmd_goodput(const char *p) {
    Goodput g;
    long long meters = 0;
    long long mps = 0;

    memset(&g, 0, sizeof g);
    if (!need_key(&p, "frame") || !take_num(&p, parse_bits, &g.frame_bits) || !need_key(&p, "data") ||
        !take_num(&p, parse_bits, &g.data_bits) || !need_key(&p, "ack") || !take_num(&p, parse_bits, &g.ack_bits) ||
        !need_key(&p, "bw") || !take_num(&p, parse_bps, &g.bps) || !need_key(&p, "dist") ||
        !take_num(&p, parse_meters, &meters) || !need_key(&p, "speed") || !take_num(&p, parse_speed, &mps) ||
        !scan_blank(p)) {
        return err("命令格式不对");
    }
    if (!span_prop(meters, mps, &g.tau_ns) || !goodput_calc(&g)) {
        return bad_span();
    }
    print_goodput(&g);
    return 0;
}

int limit_command(const char *line) {
    char cmd[32];
    const char *p = line;

    if (scan_word(&p, cmd, (int)sizeof cmd) != 1) {
        return err("空命令");
    }
    if (strcmp(cmd, "frame") == 0) {
        return cmd_frame(p);
    }
    if (strcmp(cmd, "reach") == 0) {
        return cmd_reach(p);
    }
    if (strcmp(cmd, "tau") == 0) {
        return cmd_tau(p);
    }
    if (strcmp(cmd, "delta") == 0) {
        return cmd_delta(p);
    }
    if (strcmp(cmd, "backoff") == 0 || strcmp(cmd, "picks") == 0) {
        return back_command(cmd, p);
    }
    if (strcmp(cmd, "detect") == 0) {
        return cmd_detect(p);
    }
    if (strcmp(cmd, "goodput") == 0) {
        return cmd_goodput(p);
    }
    return err("不认识的命令");
}
