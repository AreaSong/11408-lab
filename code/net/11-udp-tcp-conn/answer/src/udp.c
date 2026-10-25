#include "udp.h"

#include "sum.h"

#include <string.h>

static void write_be16(unsigned char *p, unsigned short v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static unsigned short read_be16(const unsigned char *p) {
    return (unsigned short)(((unsigned int)p[0] << 8) | p[1]);
}

const char *udp_port_name(unsigned short port) {
    switch (port) {
    case 53:
        return "DNS";
    case 67:
        return "DHCP 服务器";
    case 68:
        return "DHCP 客户";
    case 69:
        return "TFTP";
    case 123:
        return "NTP";
    case 161:
        return "SNMP";
    case 520:
        return "RIP";
    default:
        return NULL;
    }
}

int udp_eff_tenths(unsigned int app, int tcp, unsigned int *tenths) {
    unsigned int header = tcp ? 20u : 8u;
    unsigned int total;

    if (app == 0 || app > 65535u) {
        return 0;
    }
    total = app + header;
    *tenths = (app * 1000u + total / 2u) / total;
    return 1;
}

const char *udp_set(UdpGram *g, const unsigned char src[4], const unsigned char dst[4],
                    unsigned short src_port, unsigned short dst_port,
                    const unsigned char *data, int data_len) {
    if (data_len < 0 || data_len > UDP_DATA_MAX) {
        return "数据最长 256 字节";
    }
    memset(g, 0, sizeof *g);
    memcpy(g->src, src, 4);
    memcpy(g->dst, dst, 4);
    g->src_port = src_port;
    g->dst_port = dst_port;
    g->data_len = data_len;
    g->length = (unsigned short)(8 + data_len);
    if (data_len > 0) {
        memcpy(g->data, data, (size_t)data_len);
    }
    g->checksum = udp_checksum(g);
    return NULL;
}

const char *udp_read(const unsigned char *bytes, int n, UdpGram *g) {
    unsigned short length;

    if (n < 8) {
        return "UDP 首部至少 8 字节";
    }
    if (n > 8 + UDP_DATA_MAX) {
        return "UDP 数据过长";
    }
    memset(g, 0, sizeof *g);
    g->src_port = read_be16(bytes);
    g->dst_port = read_be16(bytes + 2);
    length = read_be16(bytes + 4);
    g->checksum = read_be16(bytes + 6);
    g->length = length;
    if (length < 8) {
        return "长度字段小于 8 字节";
    }
    if (n == 8 && length > 8) {
        g->partial = 1;
        g->data_len = length - 8;
        if (g->data_len > UDP_DATA_MAX) {
            return "长度字段太大";
        }
        return NULL;
    }
    if (n != (int)length) {
        return "长度字段和实际字节数不一致";
    }
    g->data_len = n - 8;
    if (g->data_len > 0) {
        memcpy(g->data, bytes + 8, (size_t)g->data_len);
    }
    return NULL;
}

int udp_cover(const UdpGram *g, int zero_checksum, unsigned char *buf, int cap) {
    int udp_len = g->length;
    int pad = (udp_len % 2) ? 1 : 0;
    int n = 12 + udp_len;

    if (g->partial || udp_len < 8 || g->data_len != udp_len - 8) {
        return -1;
    }
    if (n + pad > cap) {
        return -1;
    }
    memcpy(buf, g->src, 4);
    memcpy(buf + 4, g->dst, 4);
    buf[8] = 0;
    buf[9] = 17;
    write_be16(buf + 10, (unsigned short)udp_len);
    write_be16(buf + 12, g->src_port);
    write_be16(buf + 14, g->dst_port);
    write_be16(buf + 16, (unsigned short)udp_len);
    if (zero_checksum) {
        buf[18] = 0;
        buf[19] = 0;
    } else {
        write_be16(buf + 18, g->checksum);
    }
    if (g->data_len > 0) {
        memcpy(buf + 20, g->data, (size_t)g->data_len);
    }
    if (pad) {
        buf[n] = 0;
    }
    return n + pad;
}

unsigned short udp_checksum(const UdpGram *g) {
    unsigned char buf[12 + 8 + UDP_DATA_MAX + 1];
    int n = udp_cover(g, 1, buf, (int)sizeof buf);

    if (n < 0) {
        return 0;
    }
    return sum_finish(sum_add_bytes(0, buf, n), 1);
}

int udp_verify(const UdpGram *g) {
    unsigned char buf[12 + 8 + UDP_DATA_MAX + 1];
    int n;
    unsigned int folded;

    if (g->checksum == 0) {
        return UDP_SKIP;
    }
    n = udp_cover(g, 0, buf, (int)sizeof buf);
    if (n < 0) {
        return UDP_BAD;
    }
    folded = sum_fold(sum_add_bytes(0, buf, n));
    return folded == 0xffffu ? UDP_OK : UDP_BAD;
}
