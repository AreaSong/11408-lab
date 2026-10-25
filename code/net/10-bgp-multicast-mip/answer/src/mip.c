#include "mip.h"

#include "bytes.h"
#include "checksum.h"
#include "packet.h"

#include <string.h>

int mip_datagram(const unsigned char src[4], const unsigned char dst[4],
                 const unsigned char *payload, int payload_len, unsigned char *ip, int cap,
                 int *ip_len, char *err, int err_cap) {
    Packet pkt;

    if (payload_len < 1 || payload_len > MIP_PAYLOAD_MAX) {
        snprintf(err, (size_t)err_cap, "载荷要在 1～%d 字节", MIP_PAYLOAD_MAX);
        return 0;
    }
    memset(&pkt, 0, sizeof pkt);
    memcpy(pkt.ip.src, src, 4);
    memcpy(pkt.ip.dst, dst, 4);
    pkt.ip.ttl = 64;
    pkt.ip.id = 1;
    pkt.tcp.src_port = 50000;
    pkt.tcp.dst_port = 80;
    pkt.tcp.seq = 1;
    pkt.tcp.ack = 1;
    pkt.tcp.flags = 0x18;
    pkt.tcp.window = 65535;
    memcpy(pkt.payload, payload, (size_t)payload_len);
    pkt.payload_len = payload_len;
    if (!packet_build(&pkt, err, err_cap)) {
        return 0;
    }
    if (pkt.ip.total_len > cap) {
        snprintf(err, (size_t)err_cap, "内层数据报放不下");
        return 0;
    }
    /* 隧道里只放 IP 数据报。以太网头和填充留在帧里，不进外层。 */
    memcpy(ip, pkt.frame + ETH_HDR_LEN, pkt.ip.total_len);
    *ip_len = pkt.ip.total_len;
    return 1;
}

int mip_wrap(const unsigned char *inner, int inner_len, const unsigned char src[4],
             const unsigned char dst[4], unsigned char ttl, unsigned char *outer, int cap,
             int *outer_len, char *err, int err_cap) {
    unsigned short sum;
    int total;

    if (inner_len < IP_HDR_MIN || ttl == 0) {
        snprintf(err, (size_t)err_cap, "内层数据报或外层 TTL 不合法");
        return 0;
    }
    total = IP_HDR_MIN + inner_len;
    if (total > cap || total > 65535) {
        snprintf(err, (size_t)err_cap, "隧道数据报放不下");
        return 0;
    }
    memset(outer, 0, (size_t)total);
    outer[0] = 0x45;
    write_be16(outer + 2, (unsigned short)total);
    write_be16(outer + 4, 2);
    /* DF=1：教学例子不在隧道里再分片。 */
    write_be16(outer + 6, 0x4000);
    outer[8] = ttl;
    /* 协议号 4 是 IP-in-IP。第 01 关的名字表里没有这一项。 */
    outer[9] = 4;
    memcpy(outer + 12, src, 4);
    memcpy(outer + 16, dst, 4);
    memcpy(outer + IP_HDR_MIN, inner, (size_t)inner_len);
    sum = ip_header_checksum(outer, IP_HDR_MIN);
    write_be16(outer + 10, sum);
    *outer_len = total;
    return 1;
}

int mip_hop(unsigned char *outer, int len, char *err, int err_cap) {
    unsigned short sum;

    if (len < IP_HDR_MIN) {
        snprintf(err, (size_t)err_cap, "外层首部不够");
        return 0;
    }
    if (outer[8] == 0) {
        snprintf(err, (size_t)err_cap, "外层 TTL 已经是 0");
        return 0;
    }
    outer[8]--;
    sum = ip_header_checksum(outer, IP_HDR_MIN);
    write_be16(outer + 10, sum);
    if (outer[8] == 0) {
        snprintf(err, (size_t)err_cap, "外层 TTL 减成 0，沿途丢弃");
        return 0;
    }
    return 1;
}
