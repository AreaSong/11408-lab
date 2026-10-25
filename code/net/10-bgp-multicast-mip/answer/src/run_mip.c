#include "run_mip.h"

#include "bytes.h"
#include "mip.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int has_home;
    int has_cn;
    int has_coa;
    int away;
    int colocated;
    unsigned char home[4];
    unsigned char ha[4];
    unsigned char coa[4];
    unsigned char cn[4];
    unsigned char payload[MIP_PAYLOAD_MAX];
    int payload_len;
} Mip;

static Mip mip;

void mip_session_init(void) {
    memset(&mip, 0, sizeof mip);
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

static void ip_text(const unsigned char ip[4], char *out, int cap) {
    format_ipv4(ip, out, cap);
}

static int same_ip(const unsigned char a[4], const unsigned char b[4]) {
    return memcmp(a, b, 4) == 0;
}

static int cmd_home(char *p) {
    unsigned char home[4];
    unsigned char ha[4];
    char word[16];
    char a[16];
    char b[16];

    if (take_ip(&p, home) != 0) {
        return -1;
    }
    if (take_token(&p, word, (int)sizeof word) != 1 || strcmp(word, "agent") != 0) {
        return reject("用法：home <归属地址> agent <归属代理>");
    }
    if (take_ip(&p, ha) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("用法：home <归属地址> agent <归属代理>");
    }
    if (same_ip(home, ha)) {
        return reject("归属地址和归属代理不是同一台机器");
    }
    memcpy(mip.home, home, 4);
    memcpy(mip.ha, ha, 4);
    mip.has_home = 1;
    ip_text(home, a, (int)sizeof a);
    ip_text(ha, b, (int)sizeof b);
    printf("【归属网络】\n");
    printf("移动节点的归属地址 %s，归属代理 %s。\n", a, b);
    printf("归属地址固定。通信对端只往这个地址发。\n");
    printf("结果：归属地址 %s\n", a);
    return 0;
}

static int set_coa(const unsigned char coa[4], int colocated) {
    char text[16];

    if (!mip.has_home) {
        return reject("先写 home");
    }
    if (mip.has_coa) {
        return reject("转交地址已经定了");
    }
    if (same_ip(coa, mip.home) || same_ip(coa, mip.ha)) {
        return reject("转交地址要落在外地网络上");
    }
    memcpy(mip.coa, coa, 4);
    mip.has_coa = 1;
    mip.colocated = colocated;
    ip_text(coa, text, (int)sizeof text);
    if (colocated) {
        printf("【同址转交地址】\n");
        printf("转交地址 %s 配在移动节点自己的接口上。隧道终点是它自己，外地网络可以没有外地代理。\n",
               text);
    } else {
        printf("【外地代理】\n");
        printf("外地代理的地址 %s 同时当转交地址。隧道终点是外地代理，好几台移动节点可以共用它。\n",
               text);
    }
    printf("结果：转交地址 %s\n", text);
    return 0;
}

static int cmd_foreign(char *p) {
    unsigned char coa[4];

    if (take_ip(&p, coa) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("用法：foreign <外地代理>");
    }
    return set_coa(coa, 0);
}

static int cmd_colocated(char *p) {
    unsigned char coa[4];

    if (take_ip(&p, coa) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("用法：colocated <转交地址>");
    }
    return set_coa(coa, 1);
}

static int cmd_cn(char *p) {
    unsigned char cn[4];
    char text[16];

    if (take_ip(&p, cn) != 0) {
        return -1;
    }
    if (!rest_blank(p)) {
        return reject("用法：cn <通信对端>");
    }
    if (mip.has_home && (same_ip(cn, mip.home) || same_ip(cn, mip.ha))) {
        return reject("通信对端是另一台主机");
    }
    memcpy(mip.cn, cn, 4);
    mip.has_cn = 1;
    ip_text(cn, text, (int)sizeof text);
    printf("【通信对端】\n");
    printf("通信对端 %s。它只认识归属地址，不知道移动节点现在在哪。\n", text);
    printf("结果：通信对端 %s\n", text);
    return 0;
}

static int cmd_away(char *p) {
    char coa[16];
    char ha[16];

    if (!rest_blank(p)) {
        return reject("用法：away");
    }
    if (!mip.has_home || !mip.has_coa) {
        return reject("先写 home，以及 foreign 或 colocated");
    }
    if (mip.away) {
        return reject("已经在外地网络");
    }
    mip.away = 1;
    ip_text(mip.coa, coa, (int)sizeof coa);
    ip_text(mip.ha, ha, (int)sizeof ha);
    printf("【登记】\n");
    printf("移动节点到了外地网络，转交地址是 %s。\n", coa);
    if (mip.colocated) {
        printf("没有外地代理。移动节点自己把转交地址登记给归属代理 %s。\n", ha);
    } else {
        printf("外地代理把转交地址登记给归属代理 %s。登记的对象是归属代理。\n", ha);
    }
    printf("结果：已登记，转交地址 %s\n", coa);
    return 0;
}

static int cmd_back(char *p) {
    if (!rest_blank(p)) {
        return reject("用法：back");
    }
    if (!mip.away) {
        return reject("还在归属网络");
    }
    mip.away = 0;
    printf("【回到归属网络】\n");
    printf("向归属代理注销转交地址。之后发往归属地址的数据报按普通路由交付。\n");
    printf("结果：已注销\n");
    return 0;
}

static void show_pair(const char *os, const char *od, const char *is, const char *id) {
    printf("外层 源 %s 目的 %s\n", os, od);
    printf("内层 源 %s 目的 %s\n", is, id);
}

static void hex_header(const unsigned char *hdr) {
    int i;

    printf("外层首部：");
    for (i = 0; i < 20; i++) {
        printf("%02x%s", hdr[i], i == 19 ? "\n" : " ");
    }
}

static int send_home(void) {
    unsigned char ip[256];
    char err[160];
    char cn[16];
    char home[16];
    int ip_len = 0;

    if (!mip_datagram(mip.cn, mip.home, mip.payload, mip.payload_len, ip, (int)sizeof ip, &ip_len,
                      err, (int)sizeof err)) {
        return reject(err);
    }
    ip_text(mip.cn, cn, (int)sizeof cn);
    ip_text(mip.home, home, (int)sizeof home);
    printf("【发送】通信对端 → 移动节点\n");
    printf("移动节点还在归属网络。归属代理不截获。\n");
    show_pair(cn, home, cn, home);
    printf("没有隧道。内层数据报 %d 字节，由第 01 关的封装函数生成。\n", ip_len);
    printf("结果：直接交付，目的 %s\n", home);
    return 0;
}

static int send_away(void) {
    unsigned char inner[256];
    unsigned char outer[512];
    char err[160];
    char cn[16];
    char home[16];
    char ha[16];
    char coa[16];
    int inner_len = 0;
    int outer_len = 0;
    unsigned char ttl_before;

    if (!mip_datagram(mip.cn, mip.home, mip.payload, mip.payload_len, inner, (int)sizeof inner,
                      &inner_len, err, (int)sizeof err)) {
        return reject(err);
    }
    if (!mip_wrap(inner, inner_len, mip.ha, mip.coa, 64, outer, (int)sizeof outer, &outer_len, err,
                  (int)sizeof err)) {
        return reject(err);
    }
    ip_text(mip.cn, cn, (int)sizeof cn);
    ip_text(mip.home, home, (int)sizeof home);
    ip_text(mip.ha, ha, (int)sizeof ha);
    ip_text(mip.coa, coa, (int)sizeof coa);
    printf("【发送】通信对端 → 归属地址 %s\n", home);
    printf("第 1 跳，通信对端发出。还没有隧道。\n");
    show_pair(cn, home, cn, home);
    printf("按归属地址路由，进入归属网络。\n");
    printf("第 2 跳，归属代理 %s 截获。移动节点不在家，做 IP-in-IP。\n", ha);
    show_pair(ha, coa, cn, home);
    printf("外层协议 4，总长度 %d 字节，TTL %u，首部校验和 0x%04x。\n", outer_len, outer[8],
           read_be16(outer + 10));
    printf("内层总长度 %d 字节，TTL %u，协议 TCP。内层目的仍是归属地址。\n", inner_len, inner[8]);
    hex_header(outer);
    ttl_before = outer[8];
    if (!mip_hop(outer, outer_len, err, (int)sizeof err)) {
        return reject(err);
    }
    printf("第 3 跳，沿途路由器只看外层目的 %s。\n", coa);
    printf("外层 TTL %u 减成 %u，首部校验和重算为 0x%04x。内层 TTL 仍是 %u。\n", ttl_before, outer[8],
           read_be16(outer + 10), outer[20 + 8]);
    if (mip.colocated) {
        printf("第 4 跳，转交地址在移动节点自己的接口上。它自己拆掉外层。\n");
    } else {
        printf("第 4 跳，外地代理 %s 是隧道终点，拆掉外层。\n", coa);
        printf("交给移动节点的仍是目的为归属地址的数据报。共用转交地址时，按登记时记下的 MAC 交付。\n");
    }
    printf("结果：隧道交付，外层目的 %s，内层目的 %s\n", coa, home);
    return 0;
}

static int cmd_send(char *p) {
    char word[64];

    if (take_token(&p, word, (int)sizeof word) != 1 || !rest_blank(p)) {
        return reject("用法：send <载荷>");
    }
    if ((int)strlen(word) < 1 || (int)strlen(word) > MIP_PAYLOAD_MAX) {
        return reject("载荷要在 1～32 字节");
    }
    if (!mip.has_home || !mip.has_cn) {
        return reject("先写 home 和 cn");
    }
    if (mip.away && !mip.has_coa) {
        return reject("先登记转交地址");
    }
    memcpy(mip.payload, word, strlen(word));
    mip.payload_len = (int)strlen(word);
    return mip.away ? send_away() : send_home();
}

static int cmd_reply(char *p) {
    unsigned char ip[256];
    char err[160];
    char home[16];
    char cn[16];
    char coa[16];
    int ip_len = 0;

    if (!rest_blank(p)) {
        return reject("用法：reply");
    }
    if (!mip.away || mip.payload_len < 1) {
        return reject("先 away，再 send，然后才看回程");
    }
    if (!mip_datagram(mip.home, mip.cn, mip.payload, mip.payload_len, ip, (int)sizeof ip, &ip_len,
                      err, (int)sizeof err)) {
        return reject(err);
    }
    ip_text(mip.home, home, (int)sizeof home);
    ip_text(mip.cn, cn, (int)sizeof cn);
    ip_text(mip.coa, coa, (int)sizeof coa);
    printf("【回程】移动节点 → 通信对端\n");
    printf("源地址用归属地址 %s，不用转交地址 %s。目的 %s。\n", home, coa, cn);
    show_pair(home, cn, home, cn);
    printf("没有隧道。重新封装后的数据报 %d 字节，转交地址不出现。\n", ip_len);
    if (mip.colocated) {
        printf("去程经过归属代理再到同址转交地址，回程直接回到通信对端。这就是三角路由。\n");
    } else {
        printf("去程经过归属代理和外地代理，回程直接回到通信对端。这就是三角路由。\n");
    }
    printf("结果：直接发送，源 %s，目的 %s\n", home, cn);
    return 0;
}

int run_mip(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_token(&p, cmd, (int)sizeof cmd);

    if (rc < 0) {
        return reject("这一项太长");
    }
    if (strcmp(cmd, "home") == 0) {
        return cmd_home(p);
    }
    if (strcmp(cmd, "foreign") == 0) {
        return cmd_foreign(p);
    }
    if (strcmp(cmd, "colocated") == 0) {
        return cmd_colocated(p);
    }
    if (strcmp(cmd, "cn") == 0) {
        return cmd_cn(p);
    }
    if (strcmp(cmd, "away") == 0) {
        return cmd_away(p);
    }
    if (strcmp(cmd, "back") == 0) {
        return cmd_back(p);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(p);
    }
    if (strcmp(cmd, "reply") == 0) {
        return cmd_reply(p);
    }
    return reject("不认识的命令");
}
