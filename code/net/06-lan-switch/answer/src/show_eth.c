#include "show_eth.h"

#include <stdio.h>

static void print_hex(const char *label, const unsigned char *p, int n) {
    int i;

    printf("%s", label);
    for (i = 0; i < n; i++) {
        printf("%s%02x", i ? " " : "", p[i]);
    }
    printf("\n");
}

static const char *type_name(unsigned field) {
    if (field == 0x0800u) {
        return "IPv4";
    }
    if (field == 0x0806u) {
        return "ARP";
    }
    if (field == 0x86ddu) {
        return "IPv6";
    }
    return "上层协议";
}

static int header_len(const EthFrame *fr) {
    return fr->vlan == 0 ? 14 : 18;
}

static void show_kind(const EthFrame *fr) {
    if (fr->is_len) {
        printf("IEEE 802.3。长度字段 = %u，这是数据字节数，不含后面的填充。\n", fr->field);
        printf("%u ≤ 1500，所以这两个字节是长度，不是类型。\n", fr->field);
        return;
    }
    printf("以太网 II。类型 0x%04x ≥ 0x0600，表示里面是 %s。\n", fr->field, type_name(fr->field));
    printf("小于等于 1500 的才是 IEEE 802.3 的长度。\n");
}

static void show_tag(const EthFrame *fr) {
    unsigned tci;

    if (fr->vlan == 0) {
        return;
    }
    tci = ((unsigned)fr->pri << 13) | ((unsigned)fr->cfi << 12) | (unsigned)fr->vlan;
    printf("标签插在源 MAC 和类型之间。先读到 0x8100，才知道后面还有 2 字节 TCI。\n");
    print_hex("TPID 2 字节，第 13～14 字节：", fr->raw + 12, 2);
    printf("TCI = 0x%04x，优先级 %d，CFI %d，VLAN %d。后 12 位才是 VLAN 号。\n", tci, fr->pri,
           fr->cfi, fr->vlan);
    printf("数据字段仍要凑满 46 字节。标签是多出来的，最短帧变成 68 字节，最长帧变成 1522 字节。\n");
    printf("FCS 覆盖标签，所以插入以后要重算。\n");
}

static void show_layout(const EthFrame *fr) {
    int head = header_len(fr);
    int data_at = head + 1;

    printf("目的 MAC 6 字节，第 1～6 字节。源 MAC 6 字节，第 7～12 字节。\n");
    if (fr->vlan == 0) {
        printf("类型或长度 2 字节，第 13～14 字节。数据从第 %d 个字节开始。\n", data_at);
    } else {
        printf("类型或长度 2 字节，第 17～18 字节。数据从第 %d 个字节开始。\n", data_at);
    }
    if (fr->pad > 0) {
        printf("数据 %d 字节，不足 46 字节，填充 %d 字节 0。填充在帧里，不进上层的长度。\n",
               fr->data_len, fr->pad);
    } else {
        printf("数据 %d 字节，已经不少于 46 字节，不用填充。\n", fr->data_len);
    }
    printf("FCS 4 字节，第 %d～%d 字节，按小端附在帧尾。前导码 7 字节和帧开始定界符 1 字节不算进帧长。\n",
           fr->raw_len - 3, fr->raw_len);
}

static void show_dump(const EthFrame *fr) {
    if (fr->raw_len > 80) {
        print_hex("帧太长，只印首部：", fr->raw, header_len(fr));
        print_hex("FCS：", fr->raw + fr->raw_len - 4, 4);
        return;
    }
    print_hex("帧：", fr->raw, fr->raw_len);
}

static void show_min(const EthFrame *fr) {
    printf("不含 FCS 的部分是 %d 字节，再加 4 字节 FCS，帧长 %d 字节。\n", fr->raw_len - 4, fr->raw_len);
    if (fr->vlan == 0) {
        printf("最短帧是 64 字节：6+6+2+46+4。前导码不在这 64 字节里。\n");
        return;
    }
    printf("带标签的最短帧是 68 字节。标签不拿数据字段的 46 字节来抵。\n");
}

void show_eth(const EthFrame *fr) {
    printf("【以太网帧】\n");
    show_kind(fr);
    show_tag(fr);
    show_layout(fr);
    show_min(fr);
    show_dump(fr);
    if (fr->vlan == 0) {
        printf("结果：帧长 %d 字节，数据 %d 字节，填充 %d 字节\n", fr->raw_len, fr->data_len, fr->pad);
        return;
    }
    printf("结果：帧长 %d 字节，数据 %d 字节，填充 %d 字节，VLAN %d\n", fr->raw_len, fr->data_len,
           fr->pad, fr->vlan);
}
