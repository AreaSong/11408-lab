#include "report.h"

#include <string.h>

#include "bytes.h"
#include "checksum.h"

static void print_escaped(FILE *fp, const unsigned char *p, int n) {
    int i;

    for (i = 0; i < n; i++) {
        unsigned char c = p[i];
        if (c == '\r') {
            fputs("\\r", fp);
        } else if (c == '\n') {
            fputs("\\n", fp);
        } else if (c == '\t') {
            fputs("\\t", fp);
        } else if (c == '\\') {
            fputs("\\\\", fp);
        } else if (c >= 32 && c <= 126) {
            fputc(c, fp);
        } else {
            fprintf(fp, "\\x%02x", c);
        }
    }
}

static void print_words(FILE *fp, const unsigned char *p, int len) {
    int i;
    int col = 0;
    int any = 0;

    for (i = 0; i + 1 < len; i += 2) {
        if (col == 8) {
            fputc('\n', fp);
            col = 0;
        }
        if (any) {
            fputs(" + ", fp);
        }
        fprintf(fp, "%04x", read_be16(p + i));
        any = 1;
        col++;
    }
    if (len % 2) {
        if (any) {
            fputs(" + ", fp);
        }
        fprintf(fp, "%02x00", p[len - 1]);
    }
    fputc('\n', fp);
}

static void report_extra(FILE *fp, const char *title, const unsigned char *p, int n) {
    int i;

    if (n <= 0) {
        return;
    }
    fprintf(fp, "%s %d 字节：", title, n);
    for (i = 0; i < n; i++) {
        fprintf(fp, " %02x", p[i]);
    }
    fprintf(fp, "\n本关不逐字段解释选项。\n");
}

static const char *port_note(unsigned short port) {
    if (port == 80) {
        return "（HTTP）";
    }
    if (port == 443) {
        return "（HTTPS）";
    }
    if (port == 53) {
        return "（DNS）";
    }
    if (port == 25) {
        return "（SMTP）";
    }
    if (port == 21) {
        return "（FTP 控制）";
    }
    if (port == 20) {
        return "（FTP 数据）";
    }
    return "";
}

static void report_eth(FILE *fp, const Packet *pkt) {
    char dst[24];
    char src[24];

    format_mac(pkt->eth.dst, dst, (int)sizeof dst);
    format_mac(pkt->eth.src, src, (int)sizeof src);
    fprintf(fp, "【以太网首部】%d 字节，帧内偏移 0（第 1 个字节起）\n", ETH_HDR_LEN);
    fprintf(fp, "目的 MAC = %s（6 字节）\n", dst);
    fprintf(fp, "目的 MAC 是同一条链路上的下一跳。目的 IP 在别的网络时，下一跳是默认网关，这里填的就不是目的主机的 MAC。\n");
    fprintf(fp, "源 MAC = %s（6 字节，帧内偏移 6，第 7 个字节起）\n", src);
    fprintf(fp, "源 MAC 是发出这一帧的接口。路由器转发时换成自己出接口的 MAC。\n");
    fprintf(fp, "类型 = 0x%04x（%s）。大于等于 0x0600 是以太网 II 的类型，小于等于 1500 是 IEEE 802.3 的长度。\n",
            pkt->eth.type, eth_type_name(pkt->eth.type));
}

static void report_ip(FILE *fp, const Packet *pkt) {
    const IpHeader *ip = &pkt->ip;
    char src[20];
    char dst[20];
    unsigned reserved = (ip->flags_frag >> 15) & 1u;
    unsigned df = (ip->flags_frag >> 14) & 1u;
    unsigned mf = (ip->flags_frag >> 13) & 1u;
    unsigned frag = ip->flags_frag & 0x1fffu;
    int off = ETH_HDR_LEN;

    format_ipv4(ip->src, src, (int)sizeof src);
    format_ipv4(ip->dst, dst, (int)sizeof dst);
    fprintf(fp, "【IPv4 首部】%d 字节，帧内偏移 %d（第 %d 个字节起）\n", ip->hdr_len, off, off + 1);
    fprintf(fp, "版本 = %u，首部长度 IHL = %u，即 %u × 4 = %d 字节\n", ip->version, ip->ihl, ip->ihl, ip->hdr_len);
    fprintf(fp, "区分服务（旧称 TOS）= %u\n", ip->tos);
    fprintf(fp, "总长度 = %u 字节。这是 IP 首部加数据，不含以太网首部，也不含后面的填充。\n", ip->total_len);
    fprintf(fp, "标识 = 0x%04x\n", ip->id);
    fprintf(fp, "标志与片偏移 = 0x%04x：保留 = %u，DF = %u，MF = %u，片偏移 = %u\n",
            ip->flags_frag, reserved, df, mf, frag);
    fprintf(fp, "片偏移的单位是 8 字节。分片的长度和偏移在第 07 关再算。\n");
    fprintf(fp, "TTL = %u，帧内偏移 %d（第 %d 个字节）。每经过一个路由器减 1。\n", ip->ttl, off + 8, off + 9);
    fprintf(fp, "协议 = %u（%s），帧内偏移 %d（第 %d 个字节）。6 是 TCP，17 是 UDP，1 是 ICMP。\n",
            ip->protocol, ip_proto_name(ip->protocol), off + 9, off + 10);
    fprintf(fp, "源 IP = %s（%02x %02x %02x %02x），帧内偏移 %d（第 %d 个字节起）\n",
            src, ip->src[0], ip->src[1], ip->src[2], ip->src[3], off + 12, off + 13);
    fprintf(fp, "目的 IP = %s（%02x %02x %02x %02x），帧内偏移 %d（第 %d 个字节起）\n",
            dst, ip->dst[0], ip->dst[1], ip->dst[2], ip->dst[3], off + 16, off + 17);
    fprintf(fp, "数到目的 IP：以太网首部 14 + IP 首部里它前面的 16 字节 = %d，所以它从第 %d 个字节开始。\n",
            off + 16, off + 17);
    report_extra(fp, "IP 选项", pkt->frame + off + IP_HDR_MIN, ip->hdr_len - IP_HDR_MIN);
}

static void report_ip_sum(FILE *fp, const Packet *pkt) {
    unsigned char tmp[60];
    unsigned int sum;
    unsigned int inv;
    int n = pkt->ip.hdr_len;

    memcpy(tmp, pkt->frame + ETH_HDR_LEN, (size_t)n);
    tmp[10] = 0;
    tmp[11] = 0;
    fprintf(fp, "首部校验和：字段先按 0000 参加求和，不加伪首部。\n");
    print_words(fp, tmp, n);
    sum = checksum_fold(checksum_add(0, tmp, n));
    inv = (~sum) & 0xffffu;
    fprintf(fp, "折回低 16 位后的和 = 0x%04x，按位取反 = 0x%04x\n", sum, inv);
    fprintf(fp, "字段里的校验和 = 0x%04x，%s。\n", pkt->ip.checksum,
            inv == pkt->ip.checksum ? "一致" : "不一致");
}

static void format_flags(unsigned char flags, char *out, int cap) {
    snprintf(out, cap, "保留高 2 位 = %u，URG=%u ACK=%u PSH=%u RST=%u SYN=%u FIN=%u",
             (flags >> 6) & 0x3u, (flags >> 5) & 1u, (flags >> 4) & 1u, (flags >> 3) & 1u,
             (flags >> 2) & 1u, (flags >> 1) & 1u, flags & 1u);
}

static void report_tcp(FILE *fp, const Packet *pkt) {
    const TcpHeader *tcp = &pkt->tcp;
    char flags[96];
    int off = ETH_HDR_LEN + pkt->ip.hdr_len;

    format_flags(tcp->flags, flags, (int)sizeof flags);
    fprintf(fp, "【TCP 首部】%d 字节，帧内偏移 %d（第 %d 个字节起）\n", tcp->hdr_len, off, off + 1);
    fprintf(fp, "源端口 = %u，目的端口 = %u%s。端口在 TCP 首部，不在 IP 首部。\n",
            tcp->src_port, tcp->dst_port, port_note(tcp->dst_port));
    fprintf(fp, "序号 = 0x%08x，确认号 = 0x%08x\n", tcp->seq, tcp->ack);
    fprintf(fp, "数据偏移 = %u，即 %u × 4 = %d 字节\n", tcp->data_offset, tcp->data_offset, tcp->hdr_len);
    fprintf(fp, "标志字节 = 0x%02x：%s\n", tcp->flags, flags);
    fprintf(fp, "窗口 = %u 字节，紧急指针 = %u\n", tcp->window, tcp->urgent);
    report_extra(fp, "TCP 选项", pkt->frame + off + TCP_HDR_MIN, tcp->hdr_len - TCP_HDR_MIN);
}

static void report_tcp_pending(FILE *fp, const Packet *pkt) {
    fprintf(fp, "TCP 校验和字段 = 0x%04x。报文段没有给全，不重算。\n", pkt->tcp.checksum);
    fprintf(fp, "伪首部不在线上传输，只在报文段完整时参加 TCP 校验和。\n");
}

static void report_tcp_sum(FILE *fp, const Packet *pkt) {
    unsigned char pseudo[12];
    unsigned char seg[FRAME_MAX];
    int seg_len = pkt->ip.total_len - pkt->ip.hdr_len;
    int seg_off = ETH_HDR_LEN + pkt->ip.hdr_len;
    unsigned int sum;
    unsigned int inv;

    memcpy(pseudo, pkt->ip.src, 4);
    memcpy(pseudo + 4, pkt->ip.dst, 4);
    pseudo[8] = 0;
    pseudo[9] = IP_PROTO_TCP;
    write_be16(pseudo + 10, (unsigned short)seg_len);
    memcpy(seg, pkt->frame + seg_off, (size_t)seg_len);
    seg[16] = 0;
    seg[17] = 0;
    fprintf(fp, "TCP 校验和要加 12 字节伪首部：源 IP、目的 IP、0、协议 6、TCP 长度 %d。伪首部不传输。\n",
            seg_len);
    fprintf(fp, "伪首部：");
    print_words(fp, pseudo, 12);
    if (seg_len <= 80) {
        fprintf(fp, "报文段（校验和字段先当 0000）：");
        print_words(fp, seg, seg_len);
    } else {
        fprintf(fp, "报文段共 %d 字节，这里不逐字展开。\n", seg_len);
    }
    sum = checksum_add(0, pseudo, 12);
    sum = checksum_fold(checksum_add(sum, seg, seg_len));
    inv = (~sum) & 0xffffu;
    fprintf(fp, "折回低 16 位后的和 = 0x%04x，按位取反 = 0x%04x\n", sum, inv);
    if (inv == 0) {
        fprintf(fp, "取反得到 0。TCP 把 0 改记成 0xffff，避免和“未计算校验和”混淆。\n");
        inv = 0xffffu;
    }
    fprintf(fp, "字段里的校验和 = 0x%04x，%s。\n", pkt->tcp.checksum,
            inv == pkt->tcp.checksum ? "一致" : "不一致");
}

static void report_app(FILE *fp, const Packet *pkt) {
    int missing = pkt->payload_total - pkt->payload_len;

    fprintf(fp, "【应用层数据】\n");
    if (pkt->has_tcp) {
        fprintf(fp, "IP 总长度 %u − IP 首部 %d − TCP 首部 %d = 应用层 %d 字节。\n",
                pkt->ip.total_len, pkt->ip.hdr_len, pkt->tcp.hdr_len, pkt->payload_total);
    } else {
        fprintf(fp, "协议不是 TCP。IP 数据 = 总长度 %u − 首部 %d = %d 字节。\n",
                pkt->ip.total_len, pkt->ip.hdr_len, pkt->payload_total);
    }
    fprintf(fp, "这次能看到 %d 字节", pkt->payload_len);
    if (missing > 0) {
        fprintf(fp, "，后面 %d 字节不在给出的十六进制里。\n", missing);
    } else {
        fprintf(fp, "。\n");
    }
    fprintf(fp, "可见内容：");
    if (pkt->payload_len == 0) {
        fputs("（空）\n", fp);
        return;
    }
    print_escaped(fp, pkt->payload, pkt->payload_len);
    fputc('\n', fp);
}

static void report_hex(FILE *fp, const Packet *pkt) {
    int i;
    int n = pkt->given_len;

    fprintf(fp, "【逐字节】给出的 %d 字节", n);
    if (pkt->fcs_len > 0) {
        fprintf(fp, "，末尾 %d 字节按 FCS 看待", pkt->fcs_len);
    }
    fputc('\n', fp);
    for (i = 0; i < n; i++) {
        if (i % 16 == 0) {
            fprintf(fp, "%04x", i);
        }
        fprintf(fp, " %02x", pkt->frame[i]);
        if (i % 16 == 15 || i + 1 == n) {
            fputc('\n', fp);
        }
    }
}

static void report_result(FILE *fp, const Packet *pkt) {
    char dst[24];
    char src[24];
    char sip[20];
    char dip[20];

    format_mac(pkt->eth.dst, dst, (int)sizeof dst);
    format_mac(pkt->eth.src, src, (int)sizeof src);
    fprintf(fp, "结果：给出 %d 字节，不含 FCS 的完整帧 %d 字节，目的 MAC %s，源 MAC %s",
            pkt->given_len, pkt->implied_no_fcs, dst, src);
    if (pkt->has_ip) {
        format_ipv4(pkt->ip.src, sip, (int)sizeof sip);
        format_ipv4(pkt->ip.dst, dip, (int)sizeof dip);
        fprintf(fp, "，源 IP %s，目的 IP %s，TTL %u，协议 %s（%u）",
                sip, dip, pkt->ip.ttl, ip_proto_name(pkt->ip.protocol), pkt->ip.protocol);
    }
    if (pkt->has_tcp) {
        fprintf(fp, "，源端口 %u，目的端口 %u", pkt->tcp.src_port, pkt->tcp.dst_port);
    }
    fputc('\n', fp);
}

static void report_layers(FILE *fp, const Packet *pkt) {
    int tcp_len = pkt->tcp.hdr_len + pkt->payload_len;
    int ip_len = pkt->ip.total_len;
    int body = ip_len + pkt->pad_len;

    fprintf(fp, "【分层封装】\n");
    fprintf(fp, "应用层 PDU 是报文。这次的报文 %d 字节，交给传输层。\n", pkt->payload_len);
    fprintf(fp, "传输层 PDU 是报文段。TCP 首部 %d 字节，报文段 = %d + %d = %d 字节。\n",
            pkt->tcp.hdr_len, pkt->tcp.hdr_len, pkt->payload_len, tcp_len);
    fprintf(fp, "网络层 PDU 是 IP 数据报（分组）。IPv4 首部 %d 字节，总长度 = %d + %d = %d 字节。\n",
            pkt->ip.hdr_len, pkt->ip.hdr_len, tcp_len, ip_len);
    fprintf(fp, "数据链路层 PDU 是帧。以太网首部 %d 字节：目的 MAC 6 + 源 MAC 6 + 类型 2。\n", ETH_HDR_LEN);
    fprintf(fp, "物理层 PDU 是比特。帧在链路上先写目的 MAC，再写源 MAC。\n");
    fprintf(fp, "FCS 4 字节由网卡按 CRC 填写，本关不计算，第 03 关再做。\n");
    if (pkt->pad_len > 0) {
        fprintf(fp, "以太网数据字段最少 %d 字节。IP 总长度 %d 字节，填充 %d 字节的 0。\n",
                ETH_MIN_PAYLOAD, ip_len, pkt->pad_len);
    } else {
        fprintf(fp, "以太网数据字段最少 %d 字节。IP 总长度 %d 字节，已经够长，填充 0 字节。\n",
                ETH_MIN_PAYLOAD, ip_len);
    }
    fprintf(fp, "不含 FCS 的帧长 = %d + %d = %d 字节。含 FCS 的最短帧是 %d 字节。\n",
            ETH_HDR_LEN, body, pkt->frame_len, ETH_HDR_LEN + ETH_MIN_PAYLOAD + ETH_FCS_LEN);
}

static void report_span(FILE *fp, const Packet *pkt) {
    fprintf(fp, "IP 总长度 %u 字节。以太网数据字段最少 %d 字节，因此填充 %d 字节。\n",
            pkt->ip.total_len, ETH_MIN_PAYLOAD, pkt->pad_len);
    fprintf(fp, "不含 FCS 的完整帧 = %d + %u + %d = %d 字节。\n",
            ETH_HDR_LEN, pkt->ip.total_len, pkt->pad_len, pkt->implied_no_fcs);
    if (pkt->given_len < pkt->implied_no_fcs) {
        fprintf(fp, "给出的字节比完整帧少 %d 字节，后面的内容没有出现在这段十六进制里。\n",
                pkt->implied_no_fcs - pkt->given_len);
    } else if (pkt->fcs_len > 0) {
        fprintf(fp, "末尾 %d 字节按 FCS 看待，本关不校验 CRC。\n", pkt->fcs_len);
    } else if (pkt->given_len > pkt->implied_no_fcs) {
        fprintf(fp, "给出的字节比不含 FCS 的帧多 %d 字节。多出来的正好是 4 字节时才按 FCS 看待。\n",
                pkt->given_len - pkt->implied_no_fcs);
    }
}

void report_encap(FILE *fp, const Packet *pkt) {
    report_layers(fp, pkt);
    report_eth(fp, pkt);
    report_ip(fp, pkt);
    report_ip_sum(fp, pkt);
    report_tcp(fp, pkt);
    report_tcp_sum(fp, pkt);
    report_app(fp, pkt);
    report_hex(fp, pkt);
    report_result(fp, pkt);
}

static void report_inside(FILE *fp, const Packet *pkt) {
    report_span(fp, pkt);
    report_ip(fp, pkt);
    report_ip_sum(fp, pkt);
    if (!pkt->has_tcp) {
        report_app(fp, pkt);
        return;
    }
    report_tcp(fp, pkt);
    if (pkt->tcp_checksum_ok < 0) {
        report_tcp_pending(fp, pkt);
    } else {
        report_tcp_sum(fp, pkt);
    }
    report_app(fp, pkt);
}

void report_parse(FILE *fp, const Packet *pkt) {
    fprintf(fp, "【拆帧】\n");
    fprintf(fp, "给出 %d 字节。拆封装从外向里：先以太网，再 IP，再 TCP，最后才是应用层报文。\n",
            pkt->given_len);
    fprintf(fp, "设备拆到哪一层的头，就工作在哪一层：交换机看到 MAC，路由器看到 IP。\n");
    report_eth(fp, pkt);
    if (!pkt->has_ip) {
        fprintf(fp, "类型不是 IPv4，本关不再往里拆。\n");
    } else {
        report_inside(fp, pkt);
    }
    report_hex(fp, pkt);
    report_result(fp, pkt);
}
