#include "show.h"

#include "ip.h"

static void print_tcp(FILE *fp, const IpInfo *ip) {
    unsigned flags = ip->tcp_flags;

    fprintf(fp, "TCP 源端口 %u，目的端口 %u，序号 0x%08x，确认号 0x%08x\n", ip->src_port, ip->dst_port,
            ip->seq, ip->ack);
    fprintf(fp, "TCP 数据偏移 = %u，标志字节 = 0x%02x：", ip->tcp_off, flags);
    fprintf(fp, "FIN = %d，SYN = %d，RST = %d，PSH = %d，ACK = %d，URG = %d\n", (flags & 1) != 0,
            (flags & 2) != 0, (flags & 4) != 0, (flags & 8) != 0, (flags & 16) != 0, (flags & 32) != 0);
    if (ip->tcp_short) {
        fprintf(fp, "数据偏移是 %u，TCP 首部长于 20 字节，多出来的选项这次没捕获到。\n", ip->tcp_off);
    }
}

static void print_fields(FILE *fp, const IpInfo *ip) {
    unsigned byte0 = ((unsigned)ip->version << 4) | (unsigned)ip->ihl;
    char src[16];
    char dst[16];

    ip_fmt(ip->src, src, (int)sizeof src);
    ip_fmt(ip->dst, dst, (int)sizeof dst);
    fprintf(fp, "第 1 字节 = 0x%02x：版本 = %d，IHL = %d，首部长度 = %d 字节\n", byte0, ip->version, ip->ihl,
            ip->hdr_len);
    fprintf(fp, "第 2 字节 = 0x%02x：区分服务 = %u\n", ip->tos, ip->tos);
    fprintf(fp, "第 3～4 字节 = 0x%04x：总长度 = %u 字节，其中数据 = %u 字节\n", ip->total_len, ip->total_len,
            ip->total_len - (unsigned)ip->hdr_len);
    fprintf(fp, "第 5～6 字节 = 0x%04x：标识 = 0x%04x\n", ip->id, ip->id);
    fprintf(fp, "第 7～8 字节 = 0x%04x：保留 = %d，DF = %d，MF = %d，片偏移 = %u（%u 字节）\n", ip->flags_word,
            ip->reserved, ip->df, ip->mf, ip->frag_off, ip->frag_off * 8u);
    fprintf(fp, "第 9 字节 = %u：TTL = %u\n", ip->ttl, ip->ttl);
    fprintf(fp, "第 10 字节 = %u：协议 = %s（%u）\n", ip->protocol, ip_proto_name(ip->protocol), ip->protocol);
    if (ip->checksum_ok) {
        fprintf(fp, "第 11～12 字节 = 0x%04x：首部校验和与重算一致\n", ip->checksum);
    } else {
        fprintf(fp, "第 11～12 字节 = 0x%04x：首部校验和与重算 0x%04x 不一致\n", ip->checksum,
                ip->checksum_calc);
    }
    fprintf(fp, "第 13～16 字节：源地址 %s\n", src);
    fprintf(fp, "第 17～20 字节：目的地址 %s\n", dst);
}

static void print_tail(FILE *fp, const IpInfo *ip) {
    char src[16];
    char dst[16];

    ip_fmt(ip->src, src, (int)sizeof src);
    ip_fmt(ip->dst, dst, (int)sizeof dst);
    if (ip->total_len < ETH_PAYLOAD_MIN) {
        fprintf(fp, "总长度 %u 字节 < 46 字节，快速以太网要填充。\n", ip->total_len);
    } else {
        fprintf(fp, "总长度 %u 字节 ≥ 46 字节，快速以太网不用填充。\n", ip->total_len);
    }
    fprintf(fp, "结果：源 %s，目的 %s，总长度 %u 字节，DF = %d，MF = %d，片偏移 %u", src, dst, ip->total_len,
            ip->df, ip->mf, ip->frag_off);
    if (!ip->checksum_ok) {
        fprintf(fp, "，校验和不一致");
    }
    fputc('\n', fp);
}

void show_ip(FILE *fp, const char *title, const IpInfo *ip) {
    fprintf(fp, "【%s】\n", title);
    fprintf(fp, "多字节字段按大端读。片偏移的单位是 8 字节。\n");
    print_fields(fp, ip);
    if (ip->hdr_len > IP_HDR_MIN) {
        fprintf(fp, "选项 %d 字节。选项不参与本关的计算。\n", ip->hdr_len - IP_HDR_MIN);
    }
    if (ip->has_tcp) {
        print_tcp(fp, ip);
    } else if (ip->protocol == 6) {
        fprintf(fp, "协议是 TCP，捕获的字节不够读 TCP 首部。\n");
    }
    print_tail(fp, ip);
}

static void print_nums(FILE *fp, const int *id, int n, const char *sep) {
    int i;

    for (i = 0; i < n; i++) {
        if (i > 0) {
            fputs(sep, fp);
        }
        fprintf(fp, "%d", id[i]);
    }
}

static void show_hand(FILE *fp, const CapResult *r, const IpInfo *pkt) {
    const IpInfo *a = &pkt[r->hand[0] - 1];
    const IpInfo *b = &pkt[r->hand[1] - 1];
    const IpInfo *c = &pkt[r->hand[2] - 1];

    fprintf(fp, "分组 %d 是 SYN，序号 0x%08x。", r->hand[0], a->seq);
    fprintf(fp, "分组 %d 是 SYN+ACK，确认号 0x%08x 等于该序号加 1。", r->hand[1], b->ack);
    fprintf(fp, "分组 %d 是 ACK，序号 0x%08x，确认号 0x%08x。\n", r->hand[2], c->seq, c->ack);
    fprintf(fp, "这三步是三次握手：客户序号加 1 变成下一字节，服务器序号加 1 变成确认号。\n");
}

static void show_hops(FILE *fp, const CapResult *r) {
    char from[16];
    char to[16];

    ip_fmt(r->origin_dst, from, (int)sizeof from);
    ip_fmt(r->arrive_dst, to, (int)sizeof to);
    fprintf(fp, "标识 0x%04x：发出时 TTL = %u，捕获时 TTL = %u，经过 %u - %u = %d 个路由器。\n",
            r->matched_id, r->origin_ttl, r->arrive_ttl, r->origin_ttl, r->arrive_ttl, r->routers);
    if (r->dst_changed) {
        fprintf(fp, "目的地址从 %s 变成了 %s。私有地址进出公网要做地址转换，第 08 关再算。\n", from, to);
    }
}

void show_cap(FILE *fp, const CapResult *r, const IpInfo *pkt, uint32_t host) {
    char ip[16];

    ip_fmt(host, ip, (int)sizeof ip);
    fprintf(fp, "【对照】\n");
    fprintf(fp, "源地址等于 %s 的是分组 ", ip);
    print_nums(fp, r->from_host, r->n_host, "、");
    fprintf(fp, "，由这台主机发出。\n");
    show_hand(fp, r, pkt);
    fprintf(fp, "总长度小于 46 字节的是分组 ");
    print_nums(fp, r->padded, r->n_pad, "、");
    fprintf(fp, "。快速以太网数据字段至少 46 字节，短了要填充，填充不算进 IP 总长度。\n");
    fprintf(fp, "应用层数据从序号 0x%08x 开始。服务器分组里最大的确认号是 0x%08x，差 %d 字节。\n",
            r->data_seq, r->max_ack, r->app_bytes);
    if (r->routers >= 0) {
        show_hops(fp, r);
    }
    fprintf(fp, "结果：H 发送 ");
    print_nums(fp, r->from_host, r->n_host, " ");
    fprintf(fp, "；握手 ");
    print_nums(fp, r->hand, r->n_hand, " ");
    fprintf(fp, "；填充 ");
    print_nums(fp, r->padded, r->n_pad, " ");
    fprintf(fp, "；应用层数据 %d 字节", r->app_bytes);
    if (r->routers >= 0) {
        fprintf(fp, "；路由器 %d 个", r->routers);
    }
    fputc('\n', fp);
}
