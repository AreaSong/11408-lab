#include "common/mm_shell_stage.h"
#include "common/mm_shell_link_i.h"

#include "net/net_bus.h"
#include "net/net_delay.h"
#include "net/net_exam.h"
#include "net/net_fmt.h"
#include "net/net_frame.h"
#include "net/net_mac.h"
#include "net/net_nic.h"
#include "net/net_phy.h"
#include "net/net_slide.h"
#include "net/net_stp.h"
#include "net/net_sw.h"
#include "net/net_win.h"

#include <stdio.h>
#include <stdlib.h>
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

static int eat_word(const char **ps, char *word) {
    int n = 0;
    const char *s = *ps;
    while (*s == ' ') {
        s++;
    }
    if (sscanf(s, "%31s%n", word, &n) != 1) {
        return -1;
    }
    *ps = s + n;
    return 0;
}

static int eat_num(const char **ps, long long *num, char *unit) {
    int n = 0;
    const char *s = *ps;
    while (*s == ' ') {
        s++;
    }
    if (sscanf(s, "%lld %31s%n", num, unit, &n) != 2) {
        return -1;
    }
    *ps = s + n;
    return 0;
}

static int to_bits(long long n, const char *u, long long *out) {
    if (strcmp(u, "bit") == 0) {
        *out = n;
    } else if (strcmp(u, "B") == 0) {
        *out = n * 8;
    } else if (strcmp(u, "kB") == 0) {
        *out = n * 8000;
    } else {
        return -1;
    }
    return n < 0 ? -1 : 0;
}

static int to_bps(long long n, const char *u, long long *out) {
    long long scale = 1;
    if (strcmp(u, "kbps") == 0) {
        scale = 1000;
    } else if (strcmp(u, "Mbps") == 0) {
        scale = 1000000;
    } else if (strcmp(u, "Gbps") == 0) {
        scale = 1000000000LL;
    } else if (strcmp(u, "bps") != 0) {
        return -1;
    }
    *out = n * scale;
    return n < 0 ? -1 : 0;
}

static int to_ns(long long n, const char *u, long long *out) {
    if (strcmp(u, "ns") == 0) {
        *out = n;
    } else if (strcmp(u, "us") == 0) {
        *out = n * 1000;
    } else if (strcmp(u, "ms") == 0) {
        *out = n * 1000000LL;
    } else if (strcmp(u, "s") == 0) {
        *out = n * 1000000000LL;
    } else {
        return -1;
    }
    return n < 0 ? -1 : 0;
}

static int to_m(long long n, const char *u, long long *out) {
    if (strcmp(u, "km") == 0) {
        *out = n * 1000;
    } else if (strcmp(u, "m") == 0) {
        *out = n;
    } else {
        return -1;
    }
    return n < 0 ? -1 : 0;
}

static int cmd_delay(const char *arg, FILE *out) {
    const char *p = arg;
    long long n[4];
    long long v[4];
    char u[4][32];
    NetDelay d;
    char tx[32];
    char prop[32];
    char total[32];
    char rtt[32];
    int i;
    for (i = 0; i < 4; i++) {
        if (eat_num(&p, &n[i], u[i]) != 0) {
            return -1;
        }
    }
    if (to_bits(n[0], u[0], &v[0]) || to_bps(n[1], u[1], &v[1]) || to_m(n[2], u[2], &v[2]) ||
        strcmp(u[3], "m/s") != 0 || net_link(v[0], v[1], v[2], n[3], 0, 0, &d) != 0) {
        return -1;
    }
    net_fmt_ns(tx, (int)sizeof tx, d.tx_ns);
    net_fmt_ns(prop, (int)sizeof prop, d.prop_ns);
    net_fmt_ns(total, (int)sizeof total, d.total_ns);
    net_fmt_ns(rtt, (int)sizeof rtt, d.rtt_ns);
    fprintf(out, "delay tx=%s prop=%s total=%s rtt=%s bdp=%lld bit\n", tx, prop, total, rtt, d.bdp_bit);
    return 1;
}

static int cmd_util(const char *arg, FILE *out) {
    const char *p = arg;
    long long a, b, bw, prop, va, vb, vbw, vp;
    char ua[32], ub[32], ubw[32], up[32];
    int pct = 0;
    if (eat_num(&p, &a, ua) || eat_num(&p, &b, ub) || eat_num(&p, &bw, ubw) || eat_num(&p, &prop, up) ||
        to_bits(a, ua, &va) || to_bits(b, ub, &vb) || to_bps(bw, ubw, &vbw) || to_ns(prop, up, &vp) ||
        net_util_pct(va, vb, vbw, vp, &pct) != 0) {
        return -1;
    }
    fprintf(out, "util %d%%\n", pct);
    return 1;
}

static int cmd_goodput(const char *arg, FILE *out) {
    const char *p = arg;
    long long a, b, bw, va, vb, vbw, out_bps = 0;
    char ua[32], ub[32], ubw[32], text[32];
    if (eat_num(&p, &a, ua) || eat_num(&p, &b, ub) || eat_num(&p, &bw, ubw) || to_bits(a, ua, &va) ||
        to_bits(b, ub, &vb) || to_bps(bw, ubw, &vbw) || net_goodput(va, vb, vbw, &out_bps) != 0) {
        return -1;
    }
    net_fmt_bps(text, (int)sizeof text, out_bps);
    fprintf(out, "goodput %s\n", text);
    return 1;
}

static int cmd_need(const char *arg, FILE *out) {
    const char *p = arg;
    char proto[16], ua[32], ub[32], ubw[32], up[32], ack[16];
    long long a, b, bw, prop, va, vb, vbw, vp;
    int util = 100;
    int window = 0;
    int bits = 0;
    if (eat_word(&p, proto) || eat_num(&p, &a, ua) || eat_num(&p, &b, ub) || eat_num(&p, &bw, ubw) ||
        eat_num(&p, &prop, up) || eat_word(&p, ack)) {
        return -1;
    }
    if (strcmp(ack, "same") != 0 && strcmp(ack, "0") != 0) {
        return -1;
    }
    if (*p != '\0') {
        if (sscanf(p, "%d", &util) != 1) {
            return -1;
        }
    }
    if (to_bits(a, ua, &va) || to_bits(b, ub, &vb) || to_bps(bw, ubw, &vbw) || to_ns(prop, up, &vp) ||
        net_need(proto, va, vb, strcmp(ack, "same") == 0, vbw, vp, util, &window, &bits) != 0) {
        return -1;
    }
    fprintf(out, "need window=%d bits=%d\n", window, bits);
    return 1;
}

static int cmd_rate(const char *arg, FILE *out) {
    const char *p = arg;
    char proto[16], ub[32], ubw[32], up[32], ack[16], text[32];
    long long window, bits, bw, prop, vb, vbw, vp, vack = 0, rate = 0;
    if (eat_word(&p, proto) || sscanf(p, "%lld", &window) != 1) {
        return -1;
    }
    while (*p == ' ') {
        p++;
    }
    while (*p != '\0' && *p != ' ') {
        p++;
    }
    if (eat_num(&p, &bits, ub) || eat_num(&p, &bw, ubw) || eat_num(&p, &prop, up) || eat_word(&p, ack) ||
        to_bits(bits, ub, &vb) || to_bps(bw, ubw, &vbw) || to_ns(prop, up, &vp)) {
        return -1;
    }
    if (strcmp(ack, "0") != 0 && strcmp(ack, "same") != 0) {
        return -1;
    }
    vack = strcmp(ack, "same") == 0 ? vb : 0;
    if (net_rate(proto, (int)window, vb, vack, vbw, vp, &rate) != 0) {
        return -1;
    }
    net_fmt_bps(text, (int)sizeof text, rate);
    fprintf(out, "rate %s\n", text);
    return 1;
}


static MmMachine *need_link(MmMachine **machine, uint32_t bytes, MmShellOpt *opt, FILE *out) {
    if (*machine == NULL) {
        *machine = mm_create(bytes);
    }
    if (*machine == NULL || net_on(*machine) != 0) {
        fprintf(out, "错误：链路没有装上\n");
        return NULL;
    }
    opt->link_on = 1;
    return *machine;
}

static int cmd_nic(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    MmMachine *m = need_link(machine, bytes, opt, out);
    char op[16];
    unsigned a = 0, b = 0, c = 0, d = 0, addr = 0, len = 0;
    unsigned char raw[4];
    if (m == NULL || sscanf(arg, "%15s", op) != 1) {
        return -1;
    }
    if (strcmp(op, "put") == 0) {
        if (sscanf(arg, "%15s %u %u %u %u %u", op, &addr, &a, &b, &c, &d) != 6) {
            return -1;
        }
        raw[0] = (unsigned char)a;
        raw[1] = (unsigned char)b;
        raw[2] = (unsigned char)c;
        raw[3] = (unsigned char)d;
        if (net_put(m, addr, raw, 4) != 0) {
            return -1;
        }
        fprintf(out, "nic put ok\n");
        return 1;
    }
    if (strcmp(op, "tx") == 0) {
        if (sscanf(arg, "%15s %u %u", op, &addr, &len) != 3 || net_arm_tx(m, addr, (int)len) != 0) {
            return -1;
        }
        fprintf(out, "nic tx ok\n");
        return 1;
    }
    if (strcmp(op, "bad") == 0) {
        return net_corrupt(m, 1) == 0 ? (fprintf(out, "nic bad\n"), 1) : -1;
    }
    if (strcmp(op, "pump") == 0) {
        if (net_pump(m) != 0) {
            return -1;
        }
        fprintf(out, "nic pump ok\n");
        return 1;
    }
    if (strcmp(op, "show") == 0) {
        net_show(m, out);
        return 1;
    }
    return -1;
}

int mm_shell_link(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt) {
    int simple = mm_link_simple(line, out);
    if (simple != 0) {
        return simple;
    }
    if (starts(line, "delay")) {
        return cmd_delay(rest(line, "delay"), out) == 1 ? 1 : (fprintf(out, "错误：delay 长度 单位 带宽 单位 距离 单位 速率 m/s\n"), -1);
    }
    if (starts(line, "util")) {
        return cmd_util(rest(line, "util"), out) == 1 ? 1 : (fprintf(out, "错误：util 数据 单位 确认 单位 带宽 单位 传播 单位\n"), -1);
    }
    if (starts(line, "goodput")) {
        return cmd_goodput(rest(line, "goodput"), out) == 1 ? 1 : (fprintf(out, "错误：goodput 数据 单位 首部 单位 带宽 单位\n"), -1);
    }
    if (starts(line, "need")) {
        return cmd_need(rest(line, "need"), out) == 1 ? 1 : (fprintf(out, "错误：need 协议 短帧 单位 长帧 单位 带宽 单位 传播 单位 same|0 [利用率]\n"), -1);
    }
    if (starts(line, "rate")) {
        return cmd_rate(rest(line, "rate"), out) == 1 ? 1 : (fprintf(out, "错误：rate 协议 窗口 帧长 单位 带宽 单位 传播 单位 0|same\n"), -1);
    }
    if (starts(line, "rexmit")) {
        return mm_link_rexmit(rest(line, "rexmit"), out) == 1 ? 1 : (fprintf(out, "错误：rexmit 题号\n"), -1);
    }
    if (starts(line, "byte")) {
        char stuffed[128];
        if (net_byte_stuff(rest(line, "byte"), stuffed, (int)sizeof stuffed) != 0) {
            fprintf(out, "错误：byte 十六进制字节\n");
            return -1;
        }
        fprintf(out, "byte %s\n", stuffed);
        return 1;
    }
    if (starts(line, "span")) {
        return mm_link_span(rest(line, "span"), out) == 1 ? 1 : (fprintf(out, "错误：span 题号或 cable\n"), -1);
    }
    if (starts(line, "backoff")) {
        int n = 0;
        char text[32];
        long long ns;
        if (sscanf(rest(line, "backoff"), "%d", &n) != 1 || (ns = net_backoff_ns(n, 51200)) < 0) {
            fprintf(out, "错误：backoff 冲突次数\n");
            return -1;
        }
        net_fmt_ns(text, (int)sizeof text, ns);
        fprintf(out, "backoff %s\n", text);
        return 1;
    }
    if (starts(line, "sw")) {
        char a[64], b[64];
        const char *id = rest(line, "sw");
        int rc = strcmp(id, "2014-34") == 0 ? net_sw_2014(a, 64, b, 64) : strcmp(id, "2016-35") == 0 ? net_sw_2016(a, 64, b, 64) : -1;
        if (rc != 0) {
            fprintf(out, "错误：sw 2014-34 或 2016-35\n");
            return -1;
        }
        fprintf(out, "sw data %s\nsw ack %s\n", a, b);
        return 1;
    }
    if (starts(line, "lat")) {
        return mm_link_cut(rest(line, "lat"), out) == 1 ? 1 : (fprintf(out, "错误：lat 速率 cut 或 store 字节\n"), -1);
    }
    if (starts(line, "wifi")) {
        char text[80];
        if (net_wifi_exam(rest(line, "wifi"), text, (int)sizeof text) != 0) {
            fprintf(out, "错误：wifi 题号\n");
            return -1;
        }
        fprintf(out, "wifi %s\n", text);
        return 1;
    }
    if (starts(line, "phy")) {
        return mm_link_phy(rest(line, "phy"), out) == 1 ? 1 : (fprintf(out, "错误：phy nyquist|shannon|qam|states\n"), -1);
    }
    if (starts(line, "manch")) {
        char kind[16], bits[64], wave[128];
        long long mbps = 0, baud = 0;
        if (sscanf(rest(line, "manch"), "%15s", kind) != 1) {
            return -1;
        }
        if (strcmp(kind, "bits") == 0 && sscanf(rest(line, "manch"), "%15s %63s", kind, bits) == 2 &&
            net_manch(bits, wave, (int)sizeof wave) == 0) {
            fprintf(out, "manch %s\n", wave);
            return 1;
        }
        if (strcmp(kind, "rate") == 0 && sscanf(rest(line, "manch"), "%15s %lld", kind, &mbps) == 2 &&
            net_manch_baud(mbps * 1000000LL, &baud) == 0) {
            fprintf(out, "manch %lld MBaud\n", baud / 1000000LL);
            return 1;
        }
        fprintf(out, "错误：manch bits 或 rate\n");
        return -1;
    }
    if (starts(line, "stp")) {
        char text[64];
        if (net_stp_demo(rest(line, "stp"), text, (int)sizeof text) != 0) {
            fprintf(out, "错误：stp kruskal 或 prim\n");
            return -1;
        }
        fprintf(out, "stp %s\n", text);
        return 1;
    }
    if (starts(line, "bus")) {
        return mm_link_bus(rest(line, "bus"), out) == 1 ? 1 : (fprintf(out, "错误：bus 站数 帧数 帧长，或 bus cmp\n"), -1);
    }
    if (starts(line, "lexam")) {
        char letter[4];
        const char *id = rest(line, "lexam");
        if (net_lexam(id, letter, (int)sizeof letter) != 0) {
            fprintf(out, "错误：lexam 题号\n");
            return -1;
        }
        fprintf(out, "lexam %s %s\n", id, letter);
        return 1;
    }
    if (starts(line, "ber")) {
        unsigned seed = 0;
        int ppm = 0, bits = 0, trials = 0, flips;
        if (sscanf(rest(line, "ber"), "%u %d %d %d", &seed, &ppm, &bits, &trials) != 4 ||
            (flips = net_ber_flips(seed, ppm, bits, trials)) < 0) {
            fprintf(out, "错误：ber 种子 ppm 位数 次数\n");
            return -1;
        }
        fprintf(out, "ber flips=%d\n", flips);
        return 1;
    }
    if (starts(line, "link")) {
        const char *a = rest(line, "link");
        if (strcmp(a, "off") == 0) {
            if (*machine != NULL) {
                net_drop(*machine);
            }
            opt->link_on = 0;
            fprintf(out, "link off\n");
            return 1;
        }
        if (strcmp(a, "on") != 0 || need_link(machine, mem_bytes, opt, out) == NULL) {
            fprintf(out, "错误：link on 或 off\n");
            return -1;
        }
        fprintf(out, "link on\n");
        return 1;
    }
    if (starts(line, "nic")) {
        return cmd_nic(rest(line, "nic"), out, machine, mem_bytes, opt);
    }
    return 0;
}
