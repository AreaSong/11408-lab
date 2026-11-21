#include "net/net_mail.h"

#include "net/net_cong.h"

#include <stdio.h>
#include <string.h>

static const char *ABC = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int emit4(char *out, int cap, int o, int a, int b, int c, int d) {
    if (o + 4 >= cap) {
        return -1;
    }
    out[o] = ABC[a];
    out[o + 1] = ABC[b];
    out[o + 2] = c < 0 ? '=' : ABC[c];
    out[o + 3] = d < 0 ? '=' : ABC[d];
    return o + 4;
}

int net_b64_encode(const unsigned char *in, int n, char *out, int cap) {
    int i = 0;
    int o = 0;
    if (in == NULL || out == NULL || n < 0 || cap <= 0) {
        return -1;
    }
    while (i + 3 <= n) {
        unsigned v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8) | in[i + 2];
        o = emit4(out, cap, o, (int)((v >> 18) & 63), (int)((v >> 12) & 63), (int)((v >> 6) & 63),
                  (int)(v & 63));
        if (o < 0) {
            return -1;
        }
        i += 3;
    }
    if (i < n) {
        unsigned v = (unsigned)in[i] << 16;
        int c = -1;
        int d = -1;
        if (i + 1 < n) {
            v |= (unsigned)in[i + 1] << 8;
            c = (int)((v >> 6) & 63);
        }
        o = emit4(out, cap, o, (int)((v >> 18) & 63), (int)((v >> 12) & 63), c, d);
        if (o < 0) {
            return -1;
        }
    }
    out[o] = '\0';
    return o;
}

static int line_ok(int n, int cap) { return n > 0 && n < cap ? 0 : -1; }

int net_mail_line(const char *id, char *out, int cap) {
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2012-40") == 0) {
        /* ③ 的字节流向用户，连接仍是用户拨过去的，所以是 POP3。 */
        n = snprintf(out, (size_t)cap, "a=SMTP b=SMTP c=POP3 letter=D");
    } else if (strcmp(id, "2018-40") == 0) {
        n = snprintf(out, (size_t)cap, "body=ASCII letter=D");
    } else if (strcmp(id, "2025-40") == 0) {
        n = snprintf(out, (size_t)cap, "items=I,IV letter=A");
    } else if (strcmp(id, "b64") == 0) {
        static const unsigned char hi[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
        char text[16];
        if (net_b64_encode(hi, 6, text, (int)sizeof text) < 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "%s", text);
    } else {
        return -1;
    }
    return line_ok(n, cap);
}

int net_ftp_line(const char *id, char *out, int cap) {
    int a = 0;
    int b = 0;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2009-40") == 0) {
        n = snprintf(out, (size_t)cap, "tcp=1 ctrl=1 letter=A");
    } else if (strcmp(id, "2017-40") == 0) {
        /* 主动模式的 20 在服务器一侧。客户报上去的是临时端口。 */
        n = snprintf(out, (size_t)cap, "port20=server wrong=C");
    } else if (strcmp(id, "2023-47") == 0) {
        if (net_cong_ftp(2101, &a) != 0 || net_cong_ftp(7101, &b) != 0) {
            return -1;
        }
        /* 18000 字节占 101～18100，FIN 是 18101，第二次挥手确认 18102。6 个 RTT 是 60 ms。 */
        n = snprintf(out, (size_t)cap, "first=101 ack=18102 cwnd=%d,%d ms=60 mbps=2.4", a, b);
    } else {
        return -1;
    }
    return line_ok(n, cap);
}

static long long gcd_ll(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

static void fmt_fr(char *out, int cap, long long num, long long den) {
    long long g = gcd_ll(num, den);
    num /= g;
    den /= g;
    if (den == 1) {
        snprintf(out, (size_t)cap, "%lld", num);
    } else {
        snprintf(out, (size_t)cap, "%lld/%lld", num, den);
    }
}

static int fr_less(long long an, long long ad, long long bn, long long bd) { return an * bd < bn * ad; }

static void pick_max(long long *n, long long *d, long long bn, long long bd) {
    if (fr_less(*n, *d, bn, bd)) {
        *n = bn;
        *d = bd;
    }
}

static int p2p_pair(int peers, long long file, long long us, long long dmin, long long up, char *cs, int cc,
                    char *p2p, int pc) {
    long long nf = (long long)peers * file;
    long long sum = us + (long long)peers * up;
    long long cn = file;
    long long cd = dmin;
    long long pn = file;
    long long pd = dmin;
    if (peers < 1 || us <= 0 || dmin <= 0 || up < 0 || sum <= 0) {
        return -1;
    }
    pick_max(&cn, &cd, nf, us);
    pick_max(&pn, &pd, file, us);
    pick_max(&pn, &pd, nf, sum);
    fmt_fr(cs, cc, cn, cd);
    fmt_fr(p2p, pc, pn, pd);
    return 0;
}

int net_p2p_line(const char *id, char *out, int cap) {
    char cs[32];
    char p2p[32];
    int peers = 0;
    long long file = 300;
    long long us = 30;
    long long dmin = 10;
    long long up = 3;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "1") == 0) {
        peers = 1;
    } else if (strcmp(id, "10") == 0) {
        peers = 10;
    } else if (strcmp(id, "1000") == 0) {
        peers = 1000;
    } else if (strcmp(id, "noupload") == 0) {
        peers = 10;
        up = 0;
    } else if (strcmp(id, "slow") == 0) {
        peers = 10;
        file = 15000;
        dmin = 2;
    } else {
        return -1;
    }
    if (p2p_pair(peers, file, us, dmin, up, cs, (int)sizeof cs, p2p, (int)sizeof p2p) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "cs=%s p2p=%s", cs, p2p);
    return n > 0 && n < cap ? 0 : -1;
}

int net_aexam_line(const char *id, char *out, int cap) {
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2013-40") == 0) {
        n = snprintf(out, (size_t)cap, "items=I,II,III letter=A");
    } else if (strcmp(id, "2014-40") == 0) {
        n = snprintf(out, (size_t)cap, "smtp=0 letter=D");
    } else if (strcmp(id, "2015-33") == 0) {
        n = snprintf(out, (size_t)cap, "tcp=1 letter=D");
    } else if (strcmp(id, "2018-33") == 0) {
        n = snprintf(out, (size_t)cap, "udp=DNS letter=B");
    } else if (strcmp(id, "2019-38") == 0) {
        n = snprintf(out, (size_t)cap, "dup=3 at=t3 letter=C");
    } else if (strcmp(id, "2019-40") == 0) {
        n = snprintf(out, (size_t)cap, "direct=0 letter=B");
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
