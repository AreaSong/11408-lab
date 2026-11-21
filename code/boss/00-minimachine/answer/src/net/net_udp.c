#include "net/net_udp.h"

#include "checksum.h"

#include <stdio.h>
#include <string.h>

unsigned net_udp_field(unsigned folded) {
    unsigned inv = (~(folded & 0xffffu)) & 0xffffu;
    /* 字段里的 0 表示发送方没算。真算出 0 必须改记 0xffff。 */
    return inv == 0 ? 0xffffu : inv;
}

unsigned net_udp_fold2(unsigned hi, unsigned lo) {
    return net_udp_field(checksum_fold((hi & 0xffffu) + (lo & 0xffffu)));
}

static void put_be32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void put_be16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

int net_udp_cksum(uint32_t src, uint32_t dst, int sport, int dport, const unsigned char *data, int n,
                  unsigned *sum) {
    unsigned char pseudo[12];
    unsigned char hdr[8];
    unsigned acc;
    int ulen;
    if (sum == NULL || n < 0 || n > 256 || sport < 0 || sport > 65535 || dport < 0 || dport > 65535) {
        return -1;
    }
    if (n > 0 && data == NULL) {
        return -1;
    }
    ulen = 8 + n;
    put_be32(pseudo, src);
    put_be32(pseudo + 4, dst);
    pseudo[8] = 0;
    pseudo[9] = 17; /* UDP。TCP 的伪首部这一字节是 6。 */
    put_be16(pseudo + 10, (unsigned)ulen);
    put_be16(hdr, (unsigned)sport);
    put_be16(hdr + 2, (unsigned)dport);
    put_be16(hdr + 4, (unsigned)ulen);
    put_be16(hdr + 6, 0);
    acc = checksum_add(0, pseudo, 12);
    acc = checksum_add(acc, hdr, 8);
    if (n > 0) {
        acc = checksum_add(acc, data, n);
    }
    *sum = net_udp_field(checksum_fold(acc));
    return 0;
}

int net_udp_verify(uint32_t src, uint32_t dst, int sport, int dport, const unsigned char *data, int n) {
    unsigned char pseudo[12];
    unsigned char hdr[8];
    unsigned acc;
    unsigned field;
    int ulen;
    if (net_udp_cksum(src, dst, sport, dport, data, n, &field) != 0) {
        return 0;
    }
    ulen = 8 + n;
    put_be32(pseudo, src);
    put_be32(pseudo + 4, dst);
    pseudo[8] = 0;
    pseudo[9] = 17;
    put_be16(pseudo + 10, (unsigned)ulen);
    put_be16(hdr, (unsigned)sport);
    put_be16(hdr + 2, (unsigned)dport);
    put_be16(hdr + 4, (unsigned)ulen);
    put_be16(hdr + 6, field);
    acc = checksum_add(0, pseudo, 12);
    acc = checksum_add(acc, hdr, 8);
    if (n > 0) {
        acc = checksum_add(acc, data, n);
    }
    /* 把校验和字段加回去，回卷后应当是 0xffff。不对就丢，UDP 不因此重传。 */
    return checksum_fold(acc) == 0xffffu;
}

int net_udp_eff(int app, int *udp_tenths, int *tcp_tenths) {
    if (app <= 0 || udp_tenths == NULL || tcp_tenths == NULL) {
        return -1;
    }
    /* 伪首部不在线上，不进分母。12/20=60.0%，12/32=37.5%。 */
    *udp_tenths = app * 1000 / (app + 8);
    *tcp_tenths = app * 1000 / (app + 20);
    return 0;
}

static int hex_byte(const char *p, unsigned *out) {
    unsigned v = 0;
    int i;
    for (i = 0; i < 2; i++) {
        char c = p[i];
        unsigned d;
        if (c >= '0' && c <= '9') {
            d = (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (unsigned)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = (unsigned)(c - 'A' + 10);
        } else {
            return -1;
        }
        v = (v << 4) | d;
    }
    *out = v;
    return 0;
}

int net_udp_head(const char *hex, int *sport, int *dport, int *data, unsigned *sum) {
    unsigned b[8];
    int i;
    int ulen;
    if (hex == NULL || strlen(hex) != 16) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        if (hex_byte(hex + i * 2, &b[i]) != 0) {
            return -1;
        }
    }
    ulen = (int)((b[4] << 8) | b[5]);
    if (ulen < 8 || sport == NULL || dport == NULL || data == NULL || sum == NULL) {
        return -1;
    }
    *sport = (int)((b[0] << 8) | b[1]);
    *dport = (int)((b[2] << 8) | b[3]);
    *data = ulen - 8;
    *sum = (b[6] << 8) | b[7];
    return 0;
}

static int put_pct(char *out, int cap, int tenths) {
    return snprintf(out, (size_t)cap, "%d.%d", tenths / 10, tenths % 10);
}

int net_udp_line(const char *id, char *out, int cap) {
    int n;
    if (id == NULL || out == NULL || cap < 16) {
        return -1;
    }
    if (strcmp(id, "2024-39") == 0) {
        n = snprintf(out, (size_t)cap, "sum=%04x letter=C", net_udp_fold2(0xb9b6u, 0x65c5u));
    } else if (strcmp(id, "2018-39") == 0) {
        int sport = 0;
        int dport = 0;
        int data = 0;
        unsigned sum = 0;
        if (net_udp_head("06320045001CE217", &sport, &dport, &data, &sum) != 0 || dport != 69) {
            return -1;
        }
        /* 分用看目的端口。1586 是客户，69 是 TFTP。 */
        n = snprintf(out, (size_t)cap, "dport=%d name=TFTP data=%d from=client letter=B", dport, data);
    } else if (strcmp(id, "2021-39") == 0) {
        int udp = 0;
        int tcp = 0;
        char us[16];
        char ts[16];
        if (net_udp_eff(12, &udp, &tcp) != 0 || put_pct(us, (int)sizeof us, udp) < 0 ||
            put_pct(ts, (int)sizeof ts, tcp) < 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "udp=%s tcp=%s letter=D", us, ts);
    } else if (strcmp(id, "zero") == 0) {
        /* 折叠和已经是全 1 时，取反得到 0，线上改记 ffff。 */
        n = snprintf(out, (size_t)cap, "raw=%04x field=%04x", 0, net_udp_field(0xffffu));
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
