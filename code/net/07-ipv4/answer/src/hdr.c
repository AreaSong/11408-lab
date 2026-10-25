#include "hdr.h"

#include "scan.h"

#include <stdio.h>
#include <string.h>

static unsigned read_be16(const unsigned char *b) {
    return ((unsigned)b[0] << 8) | b[1];
}

static uint32_t read_be32(const unsigned char *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

/* 首部按 16 位一组做反码求和，校验和字段先当 0，结果再按位取反。 */
static unsigned header_checksum(const unsigned char *b, int hdr_len) {
    unsigned sum = 0;
    int i;

    for (i = 0; i + 1 < hdr_len; i += 2) {
        if (i == 10) {
            continue;
        }
        sum += read_be16(b + i);
    }
    while ((sum >> 16) != 0) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (~sum) & 0xffffu;
}

static void read_tcp(const unsigned char *tcp, IpInfo *out) {
    out->has_tcp = 1;
    out->src_port = read_be16(tcp);
    out->dst_port = read_be16(tcp + 2);
    out->seq = read_be32(tcp + 4);
    out->ack = read_be32(tcp + 8);
    out->tcp_off = tcp[12] >> 4;
    out->tcp_flags = tcp[13];
    out->tcp_short = out->tcp_off > 5;
}

int ip_parse_header(const unsigned char *b, int n, IpInfo *out, char *err, int err_cap) {
    unsigned word;

    memset(out, 0, sizeof *out);
    if (n < IP_HDR_MIN) {
        return fail_msg(err, err_cap, "至少要有 20 字节才能读 IPv4 首部");
    }
    out->version = b[0] >> 4;
    out->ihl = b[0] & 0x0f;
    out->hdr_len = out->ihl * 4;
    if (out->version != 4) {
        return fail_msg(err, err_cap, "版本不是 4");
    }
    if (out->ihl < 5 || out->hdr_len > n) {
        return fail_msg(err, err_cap, "首部长度不合法，IHL 至少为 5，而且捕获的字节要盖住首部");
    }
    out->tos = b[1];
    out->total_len = read_be16(b + 2);
    out->id = read_be16(b + 4);
    word = read_be16(b + 6);
    out->flags_word = word;
    out->reserved = (int)((word >> 15) & 1u);
    out->df = (int)((word >> 14) & 1u);
    out->mf = (int)((word >> 13) & 1u);
    out->frag_off = word & 0x1fffu;
    out->ttl = b[8];
    out->protocol = b[9];
    out->checksum = read_be16(b + 10);
    out->src = read_be32(b + 12);
    out->dst = read_be32(b + 16);
    if (out->total_len < (unsigned)out->hdr_len) {
        return fail_msg(err, err_cap, "总长度比首部还短");
    }
    out->checksum_calc = header_checksum(b, out->hdr_len);
    out->checksum_ok = out->checksum_calc == out->checksum;
    if (out->protocol == 6 && n >= out->hdr_len + 20) {
        read_tcp(b + out->hdr_len, out);
    }
    return 1;
}
