#include "udp_show.h"

#include "sum.h"

#include <stdio.h>

static void bits16(unsigned int v, char *out) {
    int i;
    int p = 0;

    for (i = 15; i >= 0; i--) {
        out[p++] = (char)('0' + ((v >> i) & 1));
        if (i > 0 && (i % 4) == 0) {
            out[p++] = ' ';
        }
    }
    out[p] = '\0';
}

static void explain_pair(unsigned int sum) {
    char text[20];

    if (sum >> 16) {
        bits16(sum & 0xffffu, text);
        printf("= 1 %s\n", text);
        printf("最高位的进位要加回最低位，不能丢掉。丢掉进位会停在选项 A 那种中间结果。\n");
        bits16(sum_fold(sum), text);
        printf("回卷后：%s\n", text);
        return;
    }
    bits16(sum, text);
    printf("= %s\n", text);
    printf("没有超过 16 位，不用回卷。\n");
}

static void write_fold(unsigned int sum) {
    printf("累加和 = 0x%x。\n", sum);
    if (sum <= 0xffffu) {
        printf("没有超过 16 位，不用回卷。\n");
        return;
    }
    while (sum >> 16) {
        unsigned int low = sum & 0xffffu;
        unsigned int carry = sum >> 16;
        unsigned int next = low + carry;
        if (next <= 0xffffu) {
            printf("低 16 位 0x%04x 加上进位 0x%x，得到 0x%04x。\n", low, carry, next);
        } else {
            printf("低 16 位 0x%04x 加上进位 0x%x，得到 0x%x。\n", low, carry, next);
        }
        sum = next;
    }
}

static void print_words(const char *label, const unsigned char *p, int nbytes) {
    int i;

    printf("%s", label);
    for (i = 0; i + 1 < nbytes; i += 2) {
        printf(" 0x%02x%02x", p[i], p[i + 1]);
    }
    printf("\n");
}

static void print_hex_bytes(const unsigned char *p, int n) {
    int i;

    for (i = 0; i < n; i++) {
        printf("%s%02x", i ? " " : "", p[i]);
    }
}

void udp_show_sum(unsigned short left, unsigned short right) {
    unsigned char raw[4];
    unsigned int sum;
    unsigned short inv;
    char text[20];

    raw[0] = (unsigned char)(left >> 8);
    raw[1] = (unsigned char)left;
    raw[2] = (unsigned char)(right >> 8);
    raw[3] = (unsigned char)right;
    sum = sum_add_bytes(0, raw, 4);
    inv = sum_finish(sum, 0);
    printf("【16 位反码求和】\n");
    bits16(left, text);
    printf("  %s\n", text);
    bits16(right, text);
    printf("+ %s\n", text);
    explain_pair(sum);
    bits16(inv, text);
    printf("再取反：%s（0x%04x）。\n", text, inv);
    printf("选项 B 是回卷后、取反前的数。题目要的是取反之后的校验和。\n");
    printf("这一步只加了题目给出的最后两个 16 位数，还不是整份伪首部。\n");
    printf("结果：%s\n", text);
}

void udp_show_build(const UdpGram *g) {
    unsigned char buf[12 + 8 + UDP_DATA_MAX + 1];
    int n = udp_cover(g, 1, buf, (int)sizeof buf);
    int pad = g->length % 2;
    unsigned int sum;
    unsigned short inv;

    printf("【UDP 校验和】%u.%u.%u.%u:%u → %u.%u.%u.%u:%u\n", g->src[0], g->src[1],
           g->src[2], g->src[3], g->src_port, g->dst[0], g->dst[1], g->dst[2], g->dst[3],
           g->dst_port);
    printf("UDP 长度 = 8 字节首部 + %d 字节数据 = %u 字节。\n", g->data_len, g->length);
    printf("长度字段填这 %u 字节，不含 12 字节伪首部。\n", g->length);
    printf("伪首部不在线上传输。它把源 IP、目的 IP、协议号 17 和 UDP 长度一起算进去。\n");
    print_words("伪首部", buf, 12);
    print_words("UDP 首部（校验和先按 0）", buf + 12, 8);
    if (g->data_len == 0) {
        printf("没有数据。\n");
    } else if (pad) {
        print_words("数据（末尾补 0，补的字节不传输）", buf + 20, n - 20);
    } else {
        print_words("数据", buf + 20, n - 20);
    }
    sum = sum_add_bytes(0, buf, n);
    write_fold(sum);
    inv = sum_finish(sum, 0);
    printf("取反得到 0x%04x。\n", inv);
    if (inv == 0) {
        printf("UDP 把算出的 0 改记为 0xffff。字段里的 0 留作「不校验」。\n");
    } else {
        printf("结果不是 0，按原样写入。只有算出 0 时才改记 0xffff。\n");
    }
    if (pad) {
        printf("数据字节数是奇数，计算时补 1 个 0 字节。长度字段仍是 %u 字节。\n", g->length);
    } else {
        printf("数据字节数是偶数，计算时不用补 0。\n");
    }
    printf("覆盖范围 %d 字节（伪首部 12 + UDP %u%s）。\n", n, g->length, pad ? " + 补 1" : "");
    printf("线上的 UDP 数据报：");
    print_hex_bytes(buf + 12, 6);
    printf(" %02x %02x", g->checksum >> 8, g->checksum & 0xff);
    if (g->data_len > 0) {
        printf(" ");
        print_hex_bytes(g->data, g->data_len);
    }
    printf("\n结果：校验和 0x%04x\n", g->checksum);
}

static void recv_ports(const UdpGram *g) {
    printf("首部：源端口 %u，目的端口 %u，长度 %u 字节，校验和字段 0x%04x。\n", g->src_port,
           g->dst_port, g->length, g->checksum);
    printf("数据 %d 字节。\n", g->data_len);
}

void udp_show_recv(const UdpGram *g) {
    unsigned short again = udp_checksum(g);
    int verdict = udp_verify(g);

    printf("【UDP 检验】%u.%u.%u.%u:%u → %u.%u.%u.%u:%u\n", g->src[0], g->src[1], g->src[2],
           g->src[3], g->src_port, g->dst[0], g->dst[1], g->dst[2], g->dst[3], g->dst_port);
    recv_ports(g);
    if (verdict == UDP_SKIP) {
        printf("校验和字段是 0，表示发送方没有计算校验和。\n");
        printf("UDP 的校验和可选。接收方不再检验，直接收下。\n");
        printf("结果：收下\n");
        return;
    }
    printf("把校验和字段按 0 重算，得到 0x%04x，与字段%s。\n", again,
           again == g->checksum ? "一致" : "不一致");
    if (verdict == UDP_OK) {
        printf("连同字段一起做 16 位反码求和，回卷后是 0xffff，表示没有差错。\n");
        printf("结果：收下\n");
        return;
    }
    printf("回卷后不是 0xffff。UDP 不重传，只丢弃。\n");
    printf("结果：丢弃\n");
}

void udp_show_header(const UdpGram *g) {
    const char *name = udp_port_name(g->dst_port);
    int data = (int)g->length - 8;

    printf("【UDP 首部】\n");
    printf("源端口 %u。\n", g->src_port);
    if (name != NULL) {
        printf("目的端口 %u，熟知端口，对应 %s。\n", g->dst_port, name);
    } else if (g->dst_port < 1024) {
        printf("目的端口 %u，是熟知端口。\n", g->dst_port);
    } else {
        printf("目的端口 %u。\n", g->dst_port);
    }
    printf("长度字段 %u 字节 = 8 字节首部 + %d 字节数据。\n", g->length, data);
    if (g->partial) {
        printf("给出的只有 8 字节首部，数据没有附在后面，不能重算校验和。\n");
    }
    printf("校验和字段是 0x%04x。\n", g->checksum);
    if (g->dst_port < 1024 && g->src_port >= 1024) {
        printf("源端口不小于 1024，目的端口是熟知端口，这是客户发给服务器。\n");
    } else if (g->src_port < 1024 && g->dst_port >= 1024) {
        printf("源端口是熟知端口，这是服务器发给客户。\n");
    }
    printf("结果：目的端口 %u，数据 %d 字节\n", g->dst_port, data);
}

void udp_show_demux(void) {
    printf("【UDP 分用】\n");
    printf("分用是接收方去掉 UDP 首部之后，把数据交给哪个应用进程。\n");
    printf("看的是目的端口号。源端口号只在对方需要回信时才有用，可以填 0。\n");
    printf("长度和校验和不标识进程。\n");
    printf("结果：目的端口号\n");
}

void udp_show_eff(unsigned int app) {
    unsigned int udp_t = 0;
    unsigned int tcp_t = 0;

    udp_eff_tenths(app, 0, &udp_t);
    udp_eff_tenths(app, 1, &tcp_t);
    printf("【传输效率】应用层数据 %u 字节\n", app);
    printf("效率 = 应用层字节 / 传输层报文总长，不算 IP 首部，也不算伪首部。\n");
    printf("UDP 首部固定 8 字节，总长 %u 字节，效率 = %u / %u = %u.%u%%。\n", app + 8, app,
           app + 8, udp_t / 10, udp_t % 10);
    printf("TCP 首部最少 20 字节，总长 %u 字节，效率 = %u / %u = %u.%u%%。\n", app + 20, app,
           app + 20, tcp_t / 10, tcp_t % 10);
    if (app == 12) {
        printf("伪首部不在报文里。12 字节若把它也算进 UDP，总长变成 32 字节，效率是 37.5%%。\n");
        printf("16.7%% 是 12/72，多算了传输层以外的首部。\n");
    }
    printf("结果：%u.%u%%，%u.%u%%\n", udp_t / 10, udp_t % 10, tcp_t / 10, tcp_t % 10);
}
