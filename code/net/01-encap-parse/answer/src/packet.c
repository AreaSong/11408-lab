#include "packet.h"

#include <stdio.h>
#include <string.h>

#include "bytes.h"
#include "checksum.h"

const char *eth_type_name(unsigned short type) {
    if (type == ETH_TYPE_IPV4) {
        return "IPv4";
    }
    if (type == ETH_TYPE_ARP) {
        return "ARP";
    }
    if (type == ETH_TYPE_IPV6) {
        return "IPv6";
    }
    /* 小于等于 1500 时这两字节是 802.3 的长度，不是以太网 II 的类型。 */
    if (type <= 1500) {
        return "IEEE 802.3 长度";
    }
    return "其他";
}

const char *ip_proto_name(unsigned char protocol) {
    if (protocol == IP_PROTO_ICMP) {
        return "ICMP";
    }
    if (protocol == IP_PROTO_TCP) {
        return "TCP";
    }
    if (protocol == IP_PROTO_UDP) {
        return "UDP";
    }
    return "其他";
}

static void write_eth(unsigned char *p, const EthHeader *eth) {
    memcpy(p, eth->dst, ETH_ADDR_LEN);
    memcpy(p + ETH_ADDR_LEN, eth->src, ETH_ADDR_LEN);
    write_be16(p + 12, eth->type);
}

static void write_ip(unsigned char *p, IpHeader *ip) {
    p[0] = (unsigned char)((ip->version << 4) | (ip->ihl & 0x0f));
    p[1] = ip->tos;
    write_be16(p + 2, ip->total_len);
    write_be16(p + 4, ip->id);
    write_be16(p + 6, ip->flags_frag);
    p[8] = ip->ttl;
    p[9] = ip->protocol;
    write_be16(p + 10, 0);
    memcpy(p + 12, ip->src, IP_ADDR_LEN);
    memcpy(p + 16, ip->dst, IP_ADDR_LEN);
    ip->checksum = ip_header_checksum(p, ip->hdr_len);
    write_be16(p + 10, ip->checksum);
}

static void write_tcp(unsigned char *p, TcpHeader *tcp, const unsigned char src[4],
                      const unsigned char dst[4], const unsigned char *payload, int payload_len) {
    int seg_len = tcp->hdr_len + payload_len;

    write_be16(p, tcp->src_port);
    write_be16(p + 2, tcp->dst_port);
    write_be32(p + 4, tcp->seq);
    write_be32(p + 8, tcp->ack);
    p[12] = (unsigned char)(tcp->data_offset << 4);
    p[13] = tcp->flags;
    write_be16(p + 14, tcp->window);
    write_be16(p + 16, 0);
    write_be16(p + 18, tcp->urgent);
    if (payload_len > 0) {
        memcpy(p + tcp->hdr_len, payload, (size_t)payload_len);
    }
    tcp->checksum = tcp_checksum(src, dst, p, seg_len);
    write_be16(p + 16, tcp->checksum);
}

static int eth_payload_len(int ip_total) {
    return ip_total < ETH_MIN_PAYLOAD ? ETH_MIN_PAYLOAD : ip_total;
}

bool packet_build(Packet *pkt, char *err, int err_cap) {
    int total;
    int payload_room;

    if (pkt->payload_len < 0 || pkt->payload_len > PAYLOAD_MAX) {
        snprintf(err, err_cap, "应用层报文最长 %d 字节", PAYLOAD_MAX);
        return false;
    }
    pkt->eth.type = ETH_TYPE_IPV4;
    pkt->ip.version = 4;
    pkt->ip.ihl = 5;
    pkt->ip.hdr_len = IP_HDR_MIN;
    pkt->ip.tos = 0;
    pkt->ip.protocol = IP_PROTO_TCP;
    /* DF=1：这一关的数据报不分片。保留位和 MF 都是 0。 */
    pkt->ip.flags_frag = 0x4000;
    pkt->tcp.data_offset = 5;
    pkt->tcp.hdr_len = TCP_HDR_MIN;
    pkt->tcp.urgent = 0;
    pkt->tcp.flags = (unsigned char)(pkt->tcp.flags & 0x3f);
    total = pkt->ip.hdr_len + pkt->tcp.hdr_len + pkt->payload_len;
    payload_room = eth_payload_len(total);
    if (ETH_HDR_LEN + payload_room > FRAME_MAX) {
        snprintf(err, err_cap, "帧超过 %d 字节", FRAME_MAX);
        return false;
    }
    pkt->ip.total_len = (unsigned short)total;
    pkt->pad_len = payload_room - total;
    pkt->frame_len = ETH_HDR_LEN + payload_room;
    pkt->given_len = pkt->frame_len;
    pkt->implied_no_fcs = pkt->frame_len;
    pkt->fcs_len = 0;
    pkt->payload_total = pkt->payload_len;
    pkt->has_ip = 1;
    pkt->has_tcp = 1;
    memset(pkt->frame, 0, (size_t)pkt->frame_len);
    write_eth(pkt->frame, &pkt->eth);
    write_ip(pkt->frame + ETH_HDR_LEN, &pkt->ip);
    write_tcp(pkt->frame + ETH_HDR_LEN + pkt->ip.hdr_len, &pkt->tcp, pkt->ip.src, pkt->ip.dst,
              pkt->payload, pkt->payload_len);
    pkt->ip_checksum_ok = 1;
    pkt->tcp_checksum_ok = 1;
    return true;
}

static void parse_eth(const unsigned char *buf, EthHeader *eth) {
    memcpy(eth->dst, buf, ETH_ADDR_LEN);
    memcpy(eth->src, buf + ETH_ADDR_LEN, ETH_ADDR_LEN);
    eth->type = read_be16(buf + 12);
}

static bool parse_ip(const unsigned char *buf, int len, IpHeader *ip, char *err, int err_cap) {
    const unsigned char *p;
    int hdr;

    if (len < ETH_HDR_LEN + IP_HDR_MIN) {
        snprintf(err, err_cap, "IPv4 首部至少 %d 字节，给出的字节不够", IP_HDR_MIN);
        return false;
    }
    p = buf + ETH_HDR_LEN;
    ip->version = (unsigned char)(p[0] >> 4);
    ip->ihl = (unsigned char)(p[0] & 0x0f);
    if (ip->version != 4) {
        snprintf(err, err_cap, "以太网类型是 IPv4，版本字段却是 %u", ip->version);
        return false;
    }
    if (ip->ihl < 5) {
        snprintf(err, err_cap, "IHL = %u，小于最小的 5", ip->ihl);
        return false;
    }
    hdr = ip->ihl * 4;
    if (len < ETH_HDR_LEN + hdr) {
        snprintf(err, err_cap, "IHL = %u，首部应有 %d 字节，给出的字节不够", ip->ihl, hdr);
        return false;
    }
    ip->hdr_len = hdr;
    ip->tos = p[1];
    ip->total_len = read_be16(p + 2);
    ip->id = read_be16(p + 4);
    ip->flags_frag = read_be16(p + 6);
    ip->ttl = p[8];
    ip->protocol = p[9];
    ip->checksum = read_be16(p + 10);
    memcpy(ip->src, p + 12, IP_ADDR_LEN);
    memcpy(ip->dst, p + 16, IP_ADDR_LEN);
    if (ip->total_len < hdr) {
        snprintf(err, err_cap, "IP 总长度 %u 小于首部 %d 字节", ip->total_len, hdr);
        return false;
    }
    return true;
}

static bool parse_tcp(const unsigned char *buf, int len, int off, TcpHeader *tcp,
                      char *err, int err_cap) {
    const unsigned char *p;
    int hdr;

    if (len < off + TCP_HDR_MIN) {
        snprintf(err, err_cap, "TCP 首部至少 %d 字节，给出的字节不够", TCP_HDR_MIN);
        return false;
    }
    p = buf + off;
    tcp->src_port = read_be16(p);
    tcp->dst_port = read_be16(p + 2);
    tcp->seq = read_be32(p + 4);
    tcp->ack = read_be32(p + 8);
    tcp->data_offset = (unsigned char)(p[12] >> 4);
    tcp->flags = p[13];
    if (tcp->data_offset < 5) {
        snprintf(err, err_cap, "TCP 数据偏移 = %u，小于最小的 5", tcp->data_offset);
        return false;
    }
    hdr = tcp->data_offset * 4;
    if (len < off + hdr) {
        snprintf(err, err_cap, "TCP 数据偏移 = %u，首部应有 %d 字节，给出的字节不够",
                 tcp->data_offset, hdr);
        return false;
    }
    tcp->hdr_len = hdr;
    tcp->window = read_be16(p + 14);
    tcp->checksum = read_be16(p + 16);
    tcp->urgent = read_be16(p + 18);
    return true;
}

static void mark_lengths(Packet *pkt) {
    int payload_room = eth_payload_len(pkt->ip.total_len);

    pkt->pad_len = payload_room - pkt->ip.total_len;
    pkt->implied_no_fcs = ETH_HDR_LEN + payload_room;
    pkt->fcs_len = (pkt->given_len == pkt->implied_no_fcs + ETH_FCS_LEN) ? ETH_FCS_LEN : 0;
}

static void copy_visible(Packet *pkt, int data_off, int data_total) {
    int end = pkt->given_len - pkt->fcs_len;
    int ip_end = ETH_HDR_LEN + pkt->ip.total_len;
    int n;

    if (end > ip_end) {
        end = ip_end;
    }
    n = end - data_off;
    if (n < 0) {
        n = 0;
    }
    if (n > data_total) {
        n = data_total;
    }
    if (n > FRAME_MAX) {
        n = FRAME_MAX;
    }
    pkt->payload_total = data_total;
    pkt->payload_len = n;
    if (n > 0) {
        memcpy(pkt->payload, pkt->frame + data_off, (size_t)n);
    }
}

static void check_tcp_sum(Packet *pkt) {
    int seg_off = ETH_HDR_LEN + pkt->ip.hdr_len;
    int seg_len = pkt->ip.total_len - pkt->ip.hdr_len;
    unsigned short calc;

    pkt->tcp_checksum_ok = -1;
    if (pkt->given_len - pkt->fcs_len < ETH_HDR_LEN + pkt->ip.total_len) {
        return;
    }
    calc = tcp_checksum(pkt->ip.src, pkt->ip.dst, pkt->frame + seg_off, seg_len);
    pkt->tcp.checksum = read_be16(pkt->frame + seg_off + 16);
    pkt->tcp_checksum_ok = calc == pkt->tcp.checksum;
}

static bool finish_tcp(Packet *pkt, char *err, int err_cap) {
    int off = ETH_HDR_LEN + pkt->ip.hdr_len;
    int seg_len = pkt->ip.total_len - pkt->ip.hdr_len;

    if (seg_len < TCP_HDR_MIN) {
        snprintf(err, err_cap, "IP 总长度装不下 TCP 首部");
        return false;
    }
    if (!parse_tcp(pkt->frame, pkt->given_len - pkt->fcs_len, off, &pkt->tcp, err, err_cap)) {
        return false;
    }
    if (seg_len < pkt->tcp.hdr_len) {
        snprintf(err, err_cap, "IP 总长度装不下 TCP 首部");
        return false;
    }
    pkt->has_tcp = 1;
    copy_visible(pkt, off + pkt->tcp.hdr_len, seg_len - pkt->tcp.hdr_len);
    check_tcp_sum(pkt);
    return true;
}

bool packet_parse(const unsigned char *buf, int len, Packet *pkt, char *err, int err_cap) {
    memset(pkt, 0, sizeof *pkt);
    pkt->ip_checksum_ok = -1;
    pkt->tcp_checksum_ok = -1;
    if (len < ETH_HDR_LEN || len > FRAME_MAX) {
        snprintf(err, err_cap, "帧字节数要在 %d 到 %d 之间", ETH_HDR_LEN, FRAME_MAX);
        return false;
    }
    memcpy(pkt->frame, buf, (size_t)len);
    pkt->given_len = len;
    pkt->frame_len = len;
    pkt->implied_no_fcs = len;
    parse_eth(buf, &pkt->eth);
    if (pkt->eth.type != ETH_TYPE_IPV4) {
        return true;
    }
    if (!parse_ip(buf, len, &pkt->ip, err, err_cap)) {
        return false;
    }
    pkt->has_ip = 1;
    pkt->ip_checksum_ok =
        ip_header_checksum(buf + ETH_HDR_LEN, pkt->ip.hdr_len) == pkt->ip.checksum;
    mark_lengths(pkt);
    if (pkt->ip.protocol != IP_PROTO_TCP) {
        copy_visible(pkt, ETH_HDR_LEN + pkt->ip.hdr_len, pkt->ip.total_len - pkt->ip.hdr_len);
        return true;
    }
    return finish_tcp(pkt, err, err_cap);
}
