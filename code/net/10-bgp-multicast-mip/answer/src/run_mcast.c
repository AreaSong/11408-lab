#include "run_mcast.h"

#include "bytes.h"
#include "mcast.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static void octet_bits(unsigned char v, char out[9]) {
    int i;

    for (i = 0; i < 8; i++) {
        out[i] = (char)((v & (1u << (7 - i))) ? '1' : '0');
    }
    out[8] = '\0';
}

static int take_ip(char **p, unsigned char ip[4]) {
    char tok[64];

    if (take_token(p, tok, (int)sizeof tok) != 1) {
        return reject("缺少 IP 地址");
    }
    if (!parse_ipv4(tok, ip)) {
        return reject("IP 地址不合法");
    }
    return 0;
}

static int cmd_map(char *p) {
    unsigned char ip[4];
    unsigned char mac[6];
    char text[16];
    char mac_text[24];
    char b0[9];
    char b1[9];
    char b2[9];
    char b3[9];

    if (take_ip(&p, ip) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("用法：map <组播 IP>");
    }
    format_ipv4(ip, text, (int)sizeof text);
    if (!mcast_map(ip, mac)) {
        return reject("224.0.0.0～239.255.255.255 才是组播地址");
    }
    format_mac(mac, mac_text, (int)sizeof mac_text);
    octet_bits(ip[0], b0);
    octet_bits(ip[1], b1);
    octet_bits(ip[2], b2);
    octet_bits(ip[3], b3);
    printf("【组播映射】%s\n", text);
    printf("D 类地址。前 4 位是 1110，后面 28 位是组标识。\n");
    printf("二进制：%s %s %s %s\n", b0, b1, b2, b3);
    printf("以太网组播 MAC 的前 24 位固定为 01-00-5e，第 25 位固定为 0，再接 IP 的低 23 位。\n");
    printf("第 2 个字节 %s 去掉最高位 %d，低 7 位是 0x%02x。\n", b1, (ip[1] >> 7) & 1, ip[1] & 0x7f);
    printf("丢掉的 5 位是第 1 个字节的低 4 位（%u）和第 2 个字节的最高位（%d）。\n", ip[0] & 0x0f,
           (ip[1] >> 7) & 1);
    if (ip[0] == 224 && ip[1] == 0 && ip[2] == 0) {
        printf("224.0.0.0/24 只在本网使用，路由器不把这一段转出去。\n");
    }
    if (ip[0] == 224 && ip[1] == 0 && ip[2] == 0 && ip[3] == 5) {
        printf("224.0.0.5 是 OSPF 路由器的组地址，映射公式和别的组播地址一样。\n");
    }
    printf("主机加入这个组用 IGMP 告诉本地路由器。组播 MAC 不能靠 ARP 问出来。\n");
    printf("结果：%s\n", mac_text);
    return 0;
}

static void print_one(const char *lead, const unsigned char ip[4]) {
    unsigned char mac[6];
    char text[16];
    char mac_text[24];

    mcast_map(ip, mac);
    format_ipv4(ip, text, (int)sizeof text);
    format_mac(mac, mac_text, (int)sizeof mac_text);
    printf("%s%s，MAC 是 %s。\n", lead, text, mac_text);
}

static int cmd_same(char *p) {
    unsigned char ip[4];
    unsigned char mac[6];
    unsigned char hi[4];
    unsigned char top[4];
    unsigned char other[4];
    char text[16];

    if (take_ip(&p, ip) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("用法：same <组播 IP>");
    }
    if (!mcast_map(ip, mac)) {
        return reject("224.0.0.0～239.255.255.255 才是组播地址");
    }
    format_ipv4(ip, text, (int)sizeof text);
    mcast_kin(ip, hi, top, other);
    printf("【同一个 MAC】%s\n", text);
    printf("映射只带走低 23 位。28 位组标识里有 5 位没进 MAC。\n");
    printf("这 5 位任意取值，2^5 = %d，所以 %d 个组播 IP 对应同一个 MAC。\n", mcast_same_count(),
           mcast_same_count());
    print_one("只把第 2 个字节的最高位翻过来：", hi);
    print_one("只改第 1 个字节的低 4 位：", top);
    print_one("改的是已经映射进去的一位：", other);
    printf("结果：%d 个\n", mcast_same_count());
    return 0;
}

int run_mcast(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_token(&p, cmd, (int)sizeof cmd);

    if (rc < 0) {
        return reject("这一项太长");
    }
    if (strcmp(cmd, "map") == 0) {
        return cmd_map(p);
    }
    if (strcmp(cmd, "same") == 0) {
        return cmd_same(p);
    }
    return reject("不认识的命令");
}
