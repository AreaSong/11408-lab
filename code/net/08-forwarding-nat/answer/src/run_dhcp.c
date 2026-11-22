#include "run_dhcp.h"

#include "addr.h"
#include "dhcp.h"
#include "line.h"

#include <stdio.h>
#include <string.h>

static void print_steps(const DhcpStep step[DHCP_STEPS]) {
    int i;

    printf("【DHCP 四步】\n");
    printf("首次获取地址。客户机还没有 IP，四步都广播，目的 IP 是 255.255.255.255。\n");
    printf("UDP 客户端口 68，服务器端口 67。收到 ACK 之前不能使用分配到的地址。\n");
    for (i = 0; i < DHCP_STEPS; i++) {
        const DhcpStep *s = &step[i];

        printf("%d. DHCP %s，%s。\n", i + 1, s->name, s->how);
        printf("源 IP %s，目的 IP %s，UDP %d → %d。\n", s->src, s->dst, s->sport, s->dport);
        printf("客户机：%s。\n", s->client);
        printf("服务器：%s。\n", s->server);
    }
}

static int cmd_dora(char *line) {
    char word[16];
    char server_text[32];
    char offer_text[32];
    uint32_t server = 0;
    uint32_t offer = 0;
    DhcpStep step[DHCP_STEPS];

    if (take_token(&line, word, (int)sizeof word) != 1 || strcmp(word, "server") != 0) {
        return reject("用法：dora server <服务器 IP> offer <分配 IP>");
    }
    if (take_token(&line, server_text, (int)sizeof server_text) != 1 || !ip_parse(server_text, &server)) {
        return reject("服务器 IP 不合法");
    }
    if (take_token(&line, word, (int)sizeof word) != 1 || strcmp(word, "offer") != 0) {
        return reject("用法：dora server <服务器 IP> offer <分配 IP>");
    }
    if (take_token(&line, offer_text, (int)sizeof offer_text) != 1 || !ip_parse(offer_text, &offer) ||
        !rest_blank(line)) {
        return reject("分配的 IP 不合法");
    }
    if (!dhcp_initial(server, offer, step)) {
        return reject("服务器和分配的地址都要是单播，而且不能相同");
    }
    print_steps(step);
    printf("结果：客户机已绑定 %s\n", offer_text);
    return 0;
}

static int cmd_frame(char *line) {
    char word[16];
    char where[16];
    int same;

    if (take_token(&line, word, (int)sizeof word) != 1 || strcmp(word, "router") != 0) {
        return reject("用法：frame router same 或 frame router other");
    }
    if (take_token(&line, where, (int)sizeof where) != 1 || !rest_blank(line)) {
        return reject("用法：frame router same 或 frame router other");
    }
    if (strcmp(where, "same") == 0) {
        same = 1;
    } else if (strcmp(where, "other") == 0) {
        same = 0;
    } else {
        return reject("router 后面写 same 或 other");
    }
    printf("【DHCP 发现报文】\n");
    printf("客户机首先发送的是 DHCP DISCOVER。源 IP 0.0.0.0，目的 IP 255.255.255.255，UDP 68 → 67。\n");
    printf("目的 IP 是受限广播，以太网目的 MAC 是 ff-ff-ff-ff-ff-ff。\n");
    if (dhcp_same_lan_hears_discover(same)) {
        printf("路由器接口和客户机在同一广播域，交换机会把这个广播帧送过来，所以能收到。\n");
        printf("路由器仍然不把 255.255.255.255 转发到别的网络。\n");
        printf("结果：DISCOVER，能收到，目的 MAC ff-ff-ff-ff-ff-ff\n");
        return 0;
    }
    printf("路由器接口不在这个广播域。受限广播过不了路由器，所以收不到。\n");
    printf("别的网段上的服务器要靠中继代理，中继改成单播再交给服务器。\n");
    printf("结果：DISCOVER，收不到\n");
    return 0;
}

int run_dhcp(char *line) {
    char cmd[16];

    if (take_token(&line, cmd, (int)sizeof cmd) != 1) {
        return reject("缺少命令");
    }
    if (strcmp(cmd, "dora") == 0) {
        return cmd_dora(line);
    }
    if (strcmp(cmd, "frame") == 0) {
        return cmd_frame(line);
    }
    return reject("不认识的命令");
}
