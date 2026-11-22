#include "show_access.h"

#include "aloha.h"
#include "csmaca.h"
#include "persist.h"
#include "qty.h"
#include "show_io.h"

#include <math.h>

static int take_g(const char **ps, double *g) {
    char word[64];
    char *end = NULL;

    if (scan_word(ps, word, (int)sizeof word) != 1) {
        return 0;
    }
    *g = strtod(word, &end);
    if (end == word || *end != '\0' || !isfinite(*g) || *g < 0 || *g > 20) {
        return 0;
    }
    return 1;
}

static int take_bits(const char **ps, int *bits, int *n, const char *stop) {
    *n = 0;
    while (1) {
        const char *save = *ps;
        char word[32];
        int rc = scan_word(ps, word, (int)sizeof word);

        if (rc == 0) {
            return 1;
        }
        if (rc < 0) {
            return 0;
        }
        if (stop != NULL && strcmp(word, stop) == 0) {
            *ps = save;
            return 1;
        }
        if ((word[0] != '0' && word[0] != '1') || word[1] != '\0' || *n >= PERSIST_MAX) {
            return 0;
        }
        bits[(*n)++] = word[0] - '0';
    }
}

static int cmd_load(const char *p, int slotted) {
    double g = 0;
    char stext[16];
    char gtext[16];
    char etext[16];
    char pure_pct[8];
    char slot_pct[8];
    char pure_s[16];
    char slot_s[16];
    int at_max = 0;

    if (!need_key(&p, "G") || !take_g(&p, &g) || !scan_blank(p)) {
        return err(slotted ? "用法：slot G <负荷>" : "用法：pure G <负荷>");
    }
    if (!aloha_s(slotted, g, stext, &at_max)) {
        return err("G 必须是 0～20 的有限数");
    }
    aloha_num(g, gtext, (int)sizeof gtext);
    aloha_num(slotted ? g : 2.0 * g, etext, (int)sizeof etext);
    aloha_peak(pure_pct, slot_pct, pure_s, slot_s);
    if (slotted) {
        printf("【时隙 ALOHA】\n");
        printf("只能在时隙起点发，部分重叠被消掉，易受攻击时间是 1 个帧时。S = G*e^(-G)。仍不听信道，但要全网对时。\n");
        printf("G = %s。S = %s*e^(-%s) = %s。\n", gtext, gtext, etext, stext);
        printf("最大在 G = 1，Smax = 1/e ≈ %s（%s）。\n", slot_s, slot_pct);
    } else {
        printf("【纯 ALOHA】\n");
        printf("不听信道，随时能发。前后各一个帧时都会撞上，易受攻击时间是 2 个帧时。S = G*e^(-2G)。\n");
        printf("G = %s。S = %s*e^(-%s) = %s。\n", gtext, gtext, etext, stext);
        printf("最大在 G = 0.5，Smax = 1/(2e) ≈ %s（%s）。\n", pure_s, pure_pct);
    }
    if (at_max) {
        printf("这个 G 就是最大点。\n");
    } else {
        printf("这个 G 不是最大点。负载再大，成功的帧反而更少。\n");
    }
    printf("结果：%s\n", stext);
    return 0;
}

static int cmd_max(const char *p) {
    char pure_pct[8];
    char slot_pct[8];
    char pure_s[16];
    char slot_s[16];

    if (!scan_blank(p)) {
        return err("max 没有参数");
    }
    aloha_peak(pure_pct, slot_pct, pure_s, slot_s);
    printf("【ALOHA 最大吞吐率】\n");
    printf("纯 ALOHA：S = G*e^(-2G)。导数为零时 G = 0.5，Smax = 1/(2e) ≈ %s，约 %s。\n", pure_s, pure_pct);
    printf("时隙 ALOHA：S = G*e^(-G)。G = 1 时 Smax = 1/e ≈ %s，约 %s。\n", slot_s, slot_pct);
    printf("两个最大点相比，时隙是纯 ALOHA 的两倍。同一个 G 下的比值是 e^G，不是常数 2。\n");
    printf("结果：1/(2e) ≈ %s，1/e ≈ %s\n", pure_pct, slot_pct);
    return 0;
}

static void persist_title(int kind) {
    if (kind == PERSIST_ONE) {
        printf("【1-坚持 CSMA】\n");
        printf("先听再发。信道空闲立刻发；信道忙就一直听，变空闲的那个时隙立刻发。这里不做冲突检测。\n");
        return;
    }
    if (kind == PERSIST_NON) {
        printf("【非坚持 CSMA】\n");
        printf("信道空闲立刻发。信道忙就不再听，等给定的时隙数再听。等待时信道空出来也不发。\n");
        return;
    }
    printf("【p-坚持 CSMA】\n");
    printf("用于时隙信道。信道忙就持续听到空闲。空闲时抽签 1 才发送，抽签 0 推迟到下一时隙。抽签代替随机的 p。\n");
}

static void persist_line(const PersistTrace *tr, const PersistStep *step) {
    if (step->action == ACT_LISTEN) {
        printf("时隙 %d：信道忙，继续听，不发送\n", step->slot);
    } else if (step->action == ACT_SEND && tr->kind == PERSIST_P) {
        printf("时隙 %d：信道空闲，抽签为 1，发送\n", step->slot);
    } else if (step->action == ACT_SEND) {
        printf("时隙 %d：信道空闲，立即发送\n", step->slot);
    } else if (step->action == ACT_STOP) {
        printf("时隙 %d：信道忙，停止监听，等待 %d 个时隙\n", step->slot, tr->wait);
    } else if (step->action == ACT_BLIND) {
        printf("时隙 %d：等待中，不听也不发\n", step->slot);
    } else {
        printf("时隙 %d：信道空闲，抽签为 0，推迟到下一时隙\n", step->slot);
    }
}

static int finish_persist(int rc, const PersistTrace *tr) {
    int i;

    if (rc == 0) {
        return err("信道序列不合法");
    }
    if (rc == -1) {
        return err("等待还没结束，信道序列太短");
    }
    if (rc == -2) {
        return err("空闲时隙比抽签多，抽签不够");
    }
    persist_title(tr->kind);
    printf("时隙从 0 编号。1 表示忙，0 表示空闲。\n");
    for (i = 0; i < tr->n; i++) {
        persist_line(tr, &tr->step[i]);
    }
    if (tr->send_slot < 0) {
        printf("结果：未发送\n");
    } else {
        printf("结果：时隙 %d 发送\n", tr->send_slot);
    }
    return 0;
}

static int cmd_one(const char *p) {
    int busy[PERSIST_MAX];
    int n = 0;
    PersistTrace tr;

    if (!take_bits(&p, busy, &n, NULL) || n < 1 || !scan_blank(p)) {
        return err("用法：one <0 或 1> ...");
    }
    return finish_persist(persist_run(PERSIST_ONE, busy, n, 0, NULL, 0, &tr), &tr);
}

static int cmd_non(const char *p) {
    int busy[PERSIST_MAX];
    int n = 0;
    int wait = 0;
    PersistTrace tr;

    if (!take_bits(&p, busy, &n, "wait") || n < 1 || !need_key(&p, "wait") || !take_int(&p, &wait, 1, 31) ||
        !scan_blank(p)) {
        return err("用法：non <0 或 1> ... wait <时隙数>");
    }
    return finish_persist(persist_run(PERSIST_NON, busy, n, wait, NULL, 0, &tr), &tr);
}

static int cmd_p(const char *p) {
    int busy[PERSIST_MAX];
    int draw[PERSIST_MAX];
    int n = 0;
    int ndraw = 0;
    PersistTrace tr;

    if (!take_bits(&p, busy, &n, "draw") || n < 1 || !need_key(&p, "draw") || !take_bits(&p, draw, &ndraw, NULL) ||
        ndraw < 1 || !scan_blank(p)) {
        return err("用法：p <0 或 1> ... draw <0 或 1> ...");
    }
    return finish_persist(persist_run(PERSIST_P, busy, n, 0, draw, ndraw, &tr), &tr);
}

static int cmd_ifs(const char *p) {
    if (!scan_blank(p)) {
        return err("ifs 没有参数");
    }
    printf("【CSMA/CA 帧间间隔】\n");
    printf("SIFS 最短，用来接上一次传输，CTS、数据帧、ACK 都用它。\n");
    printf("DIFS 更长，新发起一次传输才用，RTS 前面是它。PIFS 介于两者之间，点协调才用，这条时间线上没有。\n");
    printf("IFS1：DIFS，在 RTS 之前。预约信道是新发起的传输。\n");
    printf("IFS2：SIFS，在 CTS 之前。\n");
    printf("IFS3：SIFS，在数据帧之前。\n");
    printf("IFS4：SIFS，在 ACK 之前。\n");
    printf("DIFS 比 SIFS 长，所以四个间隔里最长的是 IFS1。\n");
    printf("结果：IFS1\n");
    return 0;
}

static int read_line_spec(const char **ps, CaSpec *in) {
    if (!need_key(ps, "difs") || !take_num(ps, parse_ns, &in->difs_ns)) {
        return 0;
    }
    if (!need_key(ps, "sifs") || !take_num(ps, parse_ns, &in->sifs_ns)) {
        return 0;
    }
    if (!need_key(ps, "slot") || !take_num(ps, parse_ns, &in->slot_ns)) {
        return 0;
    }
    if (!need_key(ps, "backoff") || !take_int(ps, &in->backoff, 0, 1023)) {
        return 0;
    }
    if (!need_key(ps, "rts") || !take_num(ps, parse_ns, &in->rts_ns)) {
        return 0;
    }
    if (!need_key(ps, "cts") || !take_num(ps, parse_ns, &in->cts_ns)) {
        return 0;
    }
    if (!need_key(ps, "data") || !take_num(ps, parse_ns, &in->data_ns)) {
        return 0;
    }
    if (!need_key(ps, "ack") || !take_num(ps, parse_ns, &in->ack_ns) || !scan_blank(*ps)) {
        return 0;
    }
    return 1;
}

static void print_timeline(const CaSpec *in, const CaTime *got) {
    char difs[32];
    char sifs[32];
    char slot[32];
    char back[32];
    char rts[32];
    char cts[32];
    char data[32];
    char ack[32];
    char nav[32];
    char total[32];

    fmt_us(difs, sizeof difs, in->difs_ns);
    fmt_us(sifs, sizeof sifs, in->sifs_ns);
    fmt_us(slot, sizeof slot, in->slot_ns);
    fmt_us(back, sizeof back, got->backoff_ns);
    fmt_us(rts, sizeof rts, in->rts_ns);
    fmt_us(cts, sizeof cts, in->cts_ns);
    fmt_us(data, sizeof data, in->data_ns);
    fmt_us(ack, sizeof ack, in->ack_ns);
    fmt_us(nav, sizeof nav, got->nav_cts_ns);
    fmt_us(total, sizeof total, got->total_ns);
    printf("【CSMA/CA 时间线】\n");
    printf("无线上听不见冲突，所以避免碰撞，不用 CSMA/CD。SIFS 比 DIFS 短，CTS 和 ACK 才能先发出去。\n");
    printf("1. 等待 DIFS %s。新发起传输，RTS 前面用它。\n", difs);
    printf("2. 退避 %d 个时隙，%d × %s = %s。DIFS 之后才倒数；信道再忙就把剩余值冻住。这里按一直空闲。\n",
           in->backoff, in->backoff, slot, back);
    printf("3. 发送 RTS %s。\n", rts);
    printf("4. 等待 SIFS %s，接收方回 CTS %s。隐藏站听到 CTS 就设置 NAV。\n", sifs, cts);
    printf("5. 等待 SIFS %s，发送数据帧 %s。\n", sifs, data);
    printf("6. 等待 SIFS %s，接收方回 ACK %s。没收到 ACK 就加大争用窗口再退避。\n", sifs, ack);
    printf("隐藏站在 CTS 结束时的 NAV = SIFS + 数据帧 + SIFS + ACK = %s。不含已经结束的 CTS，也不含更早的 DIFS 和 RTS。\n",
           nav);
    printf("结果：%s\n", total);
}

static int cmd_line(const char *p) {
    CaSpec in;
    CaTime got;

    memset(&in, 0, sizeof in);
    if (!read_line_spec(&p, &in)) {
        return err("用法：line difs <时间> sifs <时间> slot <时间> backoff <时隙数> rts <时间> cts <时间> data <时间> ack <时间>");
    }
    if (!ca_time(&in, &got)) {
        return err("DIFS 必须长于 SIFS，时间都要是正的，而且要能加得出来");
    }
    print_timeline(&in, &got);
    return 0;
}

static void print_nav(long long bits, long long bps, long long difs, long long sifs, long long rts, long long cts,
                      long long ack, long long data_ns, long long nav) {
    char bits_s[64];
    char bps_s[64];
    char difs_s[32];
    char sifs_s[32];
    char rts_s[32];
    char cts_s[32];
    char ack_s[32];
    char data_s[32];
    char nav_s[32];

    fmt_bits(bits_s, sizeof bits_s, bits);
    fmt_bps(bps_s, sizeof bps_s, bps);
    fmt_us(difs_s, sizeof difs_s, difs);
    fmt_us(sifs_s, sizeof sifs_s, sifs);
    fmt_us(rts_s, sizeof rts_s, rts);
    fmt_us(cts_s, sizeof cts_s, cts);
    fmt_us(ack_s, sizeof ack_s, ack);
    fmt_us(data_s, sizeof data_s, data_ns);
    fmt_us(nav_s, sizeof nav_s, nav);
    printf("【CSMA/CA 的 NAV】\n");
    printf("数据帧 %s，带宽 %s。\n", bits_s, bps_s);
    printf("发送时延 = %lld bit / %lld bps = %s。\n", bits, bps, data_s);
    printf("隐藏站收到 CTS 时，CTS 已经发完。后面还要占用一个 SIFS、数据帧、再一个 SIFS 和 ACK。\n");
    printf("NAV = %s + %s + %s + %s = %s。\n", sifs_s, data_s, sifs_s, ack_s, nav_s);
    printf("DIFS %s、RTS %s、CTS %s 都在这个起点之前，不加进去。\n", difs_s, rts_s, cts_s);
    printf("结果：%s\n", nav_s);
}

static int cmd_nav(const char *p) {
    long long difs = 0;
    long long sifs = 0;
    long long rts = 0;
    long long cts = 0;
    long long ack = 0;
    long long bits = 0;
    long long bps = 0;
    long long data_ns = 0;
    long long nav = 0;

    if (!need_key(&p, "difs") || !take_num(&p, parse_ns, &difs) || !need_key(&p, "sifs") ||
        !take_num(&p, parse_ns, &sifs) || !need_key(&p, "rts") || !take_num(&p, parse_ns, &rts) ||
        !need_key(&p, "cts") || !take_num(&p, parse_ns, &cts) || !need_key(&p, "ack") ||
        !take_num(&p, parse_ns, &ack) || !need_key(&p, "data") || !take_num(&p, parse_bits, &bits) ||
        !need_key(&p, "bw") || !take_num(&p, parse_bps, &bps) || !scan_blank(p)) {
        return err("用法：nav difs <时间> sifs <时间> rts <时间> cts <时间> ack <时间> data <长度> bw <速率>");
    }
    if (difs <= sifs || sifs <= 0 || rts <= 0 || cts <= 0 || ack <= 0) {
        return err("DIFS 必须长于 SIFS，控制帧的传输时间必须是正的");
    }
    if (!qty_div_ns(bits, bps, &data_ns)) {
        return err("数据帧发送时延除不尽");
    }
    if (!ca_nav(sifs, data_ns, ack, &nav)) {
        return err("NAV 加不出来");
    }
    print_nav(bits, bps, difs, sifs, rts, cts, ack, data_ns, nav);
    return 0;
}

int access_command(const char *line) {
    char cmd[16];

    if (scan_word(&line, cmd, (int)sizeof cmd) != 1) {
        return err("缺少命令");
    }
    if (strcmp(cmd, "pure") == 0) {
        return cmd_load(line, 0);
    }
    if (strcmp(cmd, "slot") == 0) {
        return cmd_load(line, 1);
    }
    if (strcmp(cmd, "max") == 0) {
        return cmd_max(line);
    }
    if (strcmp(cmd, "one") == 0) {
        return cmd_one(line);
    }
    if (strcmp(cmd, "non") == 0) {
        return cmd_non(line);
    }
    if (strcmp(cmd, "p") == 0) {
        return cmd_p(line);
    }
    if (strcmp(cmd, "ifs") == 0) {
        return cmd_ifs(line);
    }
    if (strcmp(cmd, "line") == 0) {
        return cmd_line(line);
    }
    if (strcmp(cmd, "nav") == 0) {
        return cmd_nav(line);
    }
    return err("不认识的命令");
}
