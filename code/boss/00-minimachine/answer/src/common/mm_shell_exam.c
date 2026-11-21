#include "common/mm_shell_link_i.h"

#include "net/net_bus.h"
#include "net/net_delay.h"
#include "net/net_exam.h"
#include "net/net_fmt.h"
#include "net/net_frame.h"
#include "net/net_mac.h"
#include "net/net_phy.h"
#include "net/net_slide.h"
#include "net/net_stp.h"
#include "net/net_sw.h"
#include "net/net_win.h"

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

int mm_link_rexmit(const char *arg, FILE *out) {
    int list[8];
    char note[32];
    int n = net_rexmit(arg, list, 8, note, (int)sizeof note);
    int i;
    if (n < 0) {
        return -1;
    }
    if (strcmp(arg, "2024-37") == 0) {
        fprintf(out, "rexmit %s\n", note);
        return 1;
    }
    fprintf(out, "rexmit %d:", n);
    for (i = 0; i < n; i++) {
        fprintf(out, " %d", list[i]);
    }
    fputc('\n', out);
    return 1;
}

int mm_link_span(const char *arg, FILE *out) {
    char text[32];
    if (strcmp(arg, "2009-37") == 0) {
        fprintf(out, "span 减少 %lld m\n", net_delta_m(800, 1000000000LL, 200000000LL));
        return 1;
    }
    if (strcmp(arg, "2016-36") == 0) {
        fprintf(out, "span %lld m\n", net_reach_m(512, 100000000LL, 1535, 200));
        return 1;
    }
    if (strcmp(arg, "2019-36") == 0) {
        net_fmt_ns(text, (int)sizeof text, net_one_way_ns(1024, 100000000LL));
        fprintf(out, "span %s\n", text);
        return 1;
    }
    if (strcmp(arg, "cable") == 0) {
        fprintf(out, "span %lld bit\n", net_min_bits(10000000LL, 2500, 200000000LL));
        return 1;
    }
    return -1;
}

int mm_link_cut(const char *arg, FILE *out) {
    long long mbps = 0;
    long long bytes = 6;
    char kind[16];
    char text[32];
    if (sscanf(arg, "%lld %15s", &mbps, kind) != 2) {
        return -1;
    }
    if (strcmp(kind, "store") == 0 && sscanf(arg, "%lld %15s %lld", &mbps, kind, &bytes) != 3) {
        return -1;
    }
    net_fmt_ns(text, (int)sizeof text, net_cut_ns(bytes, mbps));
    fprintf(out, "cut %s\n", text);
    return 1;
}

int mm_link_phy(const char *arg, FILE *out) {
    char kind[16];
    char name[32];
    long long a = 0;
    long long b = 0;
    long long bps = 0;
    int extra = 0;
    int approx = 0;
    if (sscanf(arg, "%15s %lld %lld", kind, &a, &b) < 2) {
        return -1;
    }
    if (strcmp(kind, "nyquist") == 0 && net_nyquist(a, (int)b, &bps) == 0) {
        net_fmt_bps(name, (int)sizeof name, bps);
        fprintf(out, "phy %s\n", name);
        return 1;
    }
    if (strcmp(kind, "shannon") == 0 && sscanf(arg, "%15s %lld %lld %d", kind, &a, &b, &extra) == 4 &&
        net_shannon(a, (int)b, extra, &bps, &approx) == 0) {
        net_fmt_bps(name, (int)sizeof name, bps);
        fprintf(out, "phy %s%s\n", approx ? "约 " : "", name);
        return 1;
    }
    if (strcmp(kind, "qam") == 0 && net_qam(a, b, name, (int)sizeof name) == 0) {
        fprintf(out, "phy %s\n", name);
        return 1;
    }
    if (strcmp(kind, "states") == 0) {
        fprintf(out, "phy %d\n", net_min_states((int)a));
        return 1;
    }
    return -1;
}

int mm_link_bus(const char *arg, FILE *out) {
    long long useful = 0;
    long long total = 0;
    long long tu = 0;
    long long tt = 0;
    char csma[32];
    char token[32];
    int n, frames, len;
    if (strcmp(arg, "cmp") == 0) {
        if (net_bus_csma(2, 2, 8, &useful, &total) != 0 || net_bus_token(2, 2, 8, 1, &tu, &tt) != 0) {
            return -1;
        }
        net_fmt_pct(csma, (int)sizeof csma, useful, total);
        net_fmt_pct(token, (int)sizeof token, tu, tt);
        fprintf(out, "bus 2 站 CSMA/CD %s、令牌 %s；", csma, token);
        if (net_bus_csma(20, 2, 8, &useful, &total) != 0 || net_bus_token(20, 2, 8, 1, &tu, &tt) != 0) {
            return -1;
        }
        net_fmt_pct(csma, (int)sizeof csma, useful, total);
        net_fmt_pct(token, (int)sizeof token, tu, tt);
        fprintf(out, "20 站 CSMA/CD %s、令牌 %s\n", csma, token);
        return 1;
    }
    if (sscanf(arg, "%d %d %d", &n, &frames, &len) != 3 || net_bus_csma(n, frames, len, &useful, &total) != 0) {
        return -1;
    }
    net_fmt_pct(csma, (int)sizeof csma, useful, total);
    fprintf(out, "bus %lld/%lld = %s\n", useful, total, csma);
    return 1;
}

int mm_link_simple(const char *s, FILE *out) {
    if (starts(s, "cmpwin")) {
        char rel[32];
        int n = 0;
        if (sscanf(rest(s, "cmpwin"), "%d", &n) != 1 || net_cmp_u(n, rel, (int)sizeof rel) != 0) {
            return -1;
        }
        fprintf(out, "cmpwin %s\n", rel);
        return 1;
    }
    if (starts(s, "room")) {
        int bits = 0, send = 0, recv = 0;
        if (sscanf(rest(s, "room"), "%d %d", &bits, &send) != 2 || net_room(bits, send, &recv) != 0) {
            return -1;
        }
        fprintf(out, "room recv=%d\n", recv);
        return 1;
    }
    if (starts(s, "limit")) {
        char proto[16];
        int bits = 0, send = 0, recv = 0;
        if (sscanf(rest(s, "limit"), "%15s %d", proto, &bits) != 2 || net_limit_eq(proto, bits, &send, &recv) != 0) {
            return -1;
        }
        fprintf(out, "limit send=%d recv=%d\n", send, recv);
        return 1;
    }
    if (starts(s, "bit")) {
        char stuffed[256];
        if (net_bit_stuff(rest(s, "bit"), stuffed, (int)sizeof stuffed) != 0) {
            return -1;
        }
        fprintf(out, "bit %s\n", stuffed);
        return 1;
    }
    if (starts(s, "crc")) {
        char bits[128], gen[64], rem[64];
        int clean = 0;
        if (sscanf(rest(s, "crc"), "%127s %63s", bits, gen) != 2 || net_crc_rem(bits, gen, rem, (int)sizeof rem, &clean) != 0) {
            return -1;
        }
        fprintf(out, "crc %s %s\n", rem, clean ? "ok" : "bad");
        return 1;
    }
    if (starts(s, "hamflip")) {
        char data[64], fixed[80], raw[64];
        int pos = 0, bad = 0;
        if (sscanf(rest(s, "hamflip"), "%63s %d", data, &pos) != 2 ||
            net_ham_flip(data, pos, &bad, fixed, (int)sizeof fixed, raw, (int)sizeof raw) != 0) {
            return -1;
        }
        fprintf(out, "hamflip pos=%d data=%s\n", bad, raw);
        return 1;
    }
    if (starts(s, "ham")) {
        int m = 0;
        if (sscanf(rest(s, "ham"), "%d", &m) != 1) {
            return -1;
        }
        fprintf(out, "ham %d\n", net_ham_need(m));
        return 1;
    }
    if (starts(s, "cdma")) {
        char got[16];
        if (net_cdma_2014(got, (int)sizeof got) != 0) {
            return -1;
        }
        fprintf(out, "cdma %s\n", got);
        return 1;
    }
    if (strcmp(s, "ca") == 0) {
        fprintf(out, "ca RTS,CTS,DATA,ACK\n");
        return 1;
    }
    if (strcmp(s, "dom") == 0) {
        int col = 0, brd = 0;
        if (net_domains(4, 1, &col, &brd) != 0) {
            return -1;
        }
        fprintf(out, "dom collision=%d broadcast=%d\n", col, brd);
        return 1;
    }
    if (strcmp(s, "vlan") == 0) {
        char miss = 0;
        int ports[8];
        int n = 0;
        if (net_vlan_2024(&miss, ports, 8, &n) != 0) {
            return -1;
        }
        fprintf(out, "vlan miss=%c\n", miss);
        return 1;
    }
    if (strcmp(s, "place") == 0) {
        char a[16], b[16];
        if (net_fit_2022(a, (int)sizeof a, b, (int)sizeof b) != 0) {
            return -1;
        }
        fprintf(out, "place dev1=%s dev2=%s\n", a, b);
        return 1;
    }
    if (strcmp(s, "lan") == 0) {
        char line[160];
        if (net_lan_line(line, (int)sizeof line) != 0) {
            return -1;
        }
        fprintf(out, "lan %s\n", line);
        return 1;
    }
    if (strcmp(s, "flood") == 0) {
        char line[32];
        if (net_flood_demo(line, (int)sizeof line) != 0) {
            return -1;
        }
        fprintf(out, "flood %s\n", line);
        return 1;
    }
    if (strcmp(s, "collide") == 0) {
        long long tau = 2000 * 1000000000LL / 200000000LL;
        long long rate = 0;
        char a[32], b[32], c[32];
        if (net_eff(12000, 12144, 512, 10000000LL, tau, &rate) != 0) {
            return -1;
        }
        net_fmt_ns(a, (int)sizeof a, tau);
        net_fmt_ns(b, (int)sizeof b, tau * 2);
        net_fmt_bps(c, (int)sizeof c, rate);
        fprintf(out, "collide min=%s max=%s rate=%s\n", a, b, c);
        return 1;
    }
    return 0;
}

