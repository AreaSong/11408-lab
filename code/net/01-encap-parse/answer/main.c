#include <stdio.h>
#include <string.h>

#include "bytes.h"
#include "packet.h"
#include "prompt.h"
#include "report.h"

static int rest_empty(const char *s, int used) {
    while (s[used] == ' ' || s[used] == '\t') {
        used++;
    }
    return s[used] == '\0';
}

static int next_line(char *buf, int cap, const char *tip) {
    size_t n;

    prompt("%s", tip);
    if (!fgets(buf, cap, stdin)) {
        return 0;
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        return -1;
    }
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    return 1;
}

static int read_payload(Packet *pkt, char *err, int err_cap) {
    char line[8192];
    int n = 0;
    int rc = next_line(line, (int)sizeof line, "请输入应用层报文（一行）：");

    if (rc == 0) {
        snprintf(err, err_cap, "encap 缺少应用层报文");
        return 0;
    }
    if (rc < 0) {
        snprintf(err, err_cap, "应用层报文这一行太长");
        return 0;
    }
    if (!unescape(line, pkt->payload, PAYLOAD_MAX, &n, err, err_cap)) {
        return 0;
    }
    pkt->payload_len = n;
    return 1;
}

static int read_macs(Packet *pkt, char *err, int err_cap) {
    char line[256];
    char dst[64];
    char src[64];
    int used = 0;
    int rc = next_line(line, (int)sizeof line, "请输入「目的 MAC 源 MAC」：");

    if (rc <= 0) {
        snprintf(err, err_cap, "encap 缺少 MAC");
        return 0;
    }
    if (sscanf(line, "%63s %63s %n", dst, src, &used) != 2 || !rest_empty(line, used) ||
        !parse_mac(dst, pkt->eth.dst) || !parse_mac(src, pkt->eth.src)) {
        snprintf(err, err_cap, "MAC 要写成 00-11-22-33-44-55，先目的、后源");
        return 0;
    }
    return 1;
}

static int read_ip_line(Packet *pkt, char *err, int err_cap) {
    char line[256];
    char src[64];
    char dst[64];
    char ttl_text[64];
    char id_text[64];
    unsigned int ttl = 0;
    unsigned int id = 0;
    int used = 0;
    int rc = next_line(line, (int)sizeof line, "请输入「源 IP 目的 IP TTL 标识」：");

    if (rc <= 0) {
        snprintf(err, err_cap, "encap 缺少 IP");
        return 0;
    }
    if (sscanf(line, "%63s %63s %63s %63s %n", src, dst, ttl_text, id_text, &used) != 4 ||
        !rest_empty(line, used) || !parse_ipv4(src, pkt->ip.src) || !parse_ipv4(dst, pkt->ip.dst) ||
        !parse_u32(ttl_text, &ttl) || ttl > 255u || !parse_u32(id_text, &id) || id > 65535u) {
        snprintf(err, err_cap, "IP 行要是「源 IP 目的 IP TTL 标识」，TTL 不超过 255");
        return 0;
    }
    pkt->ip.ttl = (unsigned char)ttl;
    pkt->ip.id = (unsigned short)id;
    return 1;
}

static int read_tcp_line(Packet *pkt, char *err, int err_cap) {
    char line[256];
    char sport[64];
    char dport[64];
    char seq[64];
    char ack[64];
    char flags[64];
    char window[64];
    unsigned int sport_v = 0;
    unsigned int dport_v = 0;
    unsigned int seq_v = 0;
    unsigned int ack_v = 0;
    unsigned int window_v = 0;
    int used = 0;
    int rc = next_line(line, (int)sizeof line, "请输入「源端口 目的端口 序号 确认号 标志 窗口」：");

    if (rc <= 0) {
        snprintf(err, err_cap, "encap 缺少 TCP");
        return 0;
    }
    if (sscanf(line, "%63s %63s %63s %63s %63s %63s %n", sport, dport, seq, ack, flags, window,
               &used) != 6 ||
        !rest_empty(line, used) || !parse_u32(sport, &sport_v) || sport_v > 65535u ||
        !parse_u32(dport, &dport_v) || dport_v > 65535u || !parse_u32(seq, &seq_v) ||
        !parse_u32(ack, &ack_v) || !parse_tcp_flags(flags, &pkt->tcp.flags) ||
        !parse_u32(window, &window_v) || window_v > 65535u) {
        snprintf(err, err_cap, "TCP 行要是「源端口 目的端口 序号 确认号 标志 窗口」");
        return 0;
    }
    pkt->tcp.src_port = (unsigned short)sport_v;
    pkt->tcp.dst_port = (unsigned short)dport_v;
    pkt->tcp.seq = seq_v;
    pkt->tcp.ack = ack_v;
    pkt->tcp.window = (unsigned short)window_v;
    return 1;
}

static int run_encap(void) {
    Packet pkt;
    char err[160];

    memset(&pkt, 0, sizeof pkt);
    if (!read_payload(&pkt, err, (int)sizeof err) || !read_macs(&pkt, err, (int)sizeof err) ||
        !read_ip_line(&pkt, err, (int)sizeof err) || !read_tcp_line(&pkt, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    if (!packet_build(&pkt, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    report_encap(stdout, &pkt);
    return 0;
}

static int run_parse(void) {
    char line[64];
    unsigned char buf[FRAME_MAX];
    Packet pkt;
    char err[160];
    int n = 0;
    int bad = 0;
    int rc = next_line(line, (int)sizeof line, "请输入字节数：");

    if (rc <= 0 || sscanf(line, "%d", &n) != 1 || n < ETH_HDR_LEN || n > FRAME_MAX) {
        fprintf(stderr, "parse 的字节数要在 %d 到 %d 之间\n", ETH_HDR_LEN, FRAME_MAX);
        return 1;
    }
    prompt("请输入 %d 个十六进制字节：", n);
    rc = read_hex_bytes(stdin, buf, n, &bad);
    if (rc == 0) {
        fprintf(stderr, "十六进制在第 %d 个字节之前结束了\n", bad);
        return 1;
    }
    if (rc < 0) {
        fprintf(stderr, "第 %d 个字节不是两位十六进制\n", bad);
        return 1;
    }
    if (!packet_parse(buf, n, &pkt, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    report_parse(stdout, &pkt);
    return 0;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "encap") == 0) {
        return run_encap();
    }
    if (strcmp(cmd, "parse") == 0) {
        return run_parse();
    }
    fprintf(stderr, "未知命令：%s\n", cmd);
    return 1;
}

int main(void) {
    char line[64];
    int first = 1;

    while (1) {
        char cmd[32];
        int used = 0;
        int rc = next_line(line, (int)sizeof line, "请输入命令（encap / parse），Ctrl+D 结束：");
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            fprintf(stderr, "命令这一行太长\n");
            return 1;
        }
        if (line[0] == '\0') {
            continue;
        }
        if (sscanf(line, "%31s %n", cmd, &used) != 1 || !rest_empty(line, used)) {
            fprintf(stderr, "命令只能是 encap 或 parse\n");
            return 1;
        }
        if (!first) {
            fputc('\n', stdout);
        }
        first = 0;
        if (dispatch(cmd) != 0) {
            return 1;
        }
    }
    return 0;
}
