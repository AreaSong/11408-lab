#include "run_arp.h"

#include "addr.h"
#include "arp.h"
#include "line.h"

#include <stdio.h>
#include <string.h>

static ArpCache cache;
static uint32_t host_ip;
static uint8_t host_mac[6];
static int has_host;
static int pending;
static uint32_t pending_ip;
static VlanHost vlan_tab[VLAN_MAX];
static int vlan_n;
static int observer;

static int cmd_host(char *line) {
    char ip_tok[32];
    char mac_tok[32];
    char ip[16];
    char mac[24];

    if (take_token(&line, ip_tok, (int)sizeof ip_tok) != 1 || !ip_parse(ip_tok, &host_ip)) {
        return reject("用法：host <IP> <MAC>");
    }
    if (take_token(&line, mac_tok, (int)sizeof mac_tok) != 1 || !mac_parse(mac_tok, host_mac) ||
        !rest_blank(line)) {
        return reject("MAC 写成 00-11-22-33-44-55");
    }
    has_host = 1;
    pending = 0;
    ip_fmt(host_ip, ip, (int)sizeof ip);
    mac_fmt(host_mac, mac, (int)sizeof mac);
    printf("【本机】\n");
    printf("IP %s，MAC %s\n", ip, mac);
    printf("结果：本机 %s\n", ip);
    return 0;
}

static int ask_resolve(uint32_t dst, uint32_t gw, int prefix);

static int cmd_ask(char *line) {
    char dst_tok[32];
    char gw_word[8];
    char gw_tok[32];
    uint32_t dst;
    uint32_t gw;
    int prefix;

    if (!has_host) {
        return reject("先用 host 给出本机的 IP 和 MAC");
    }
    if (take_token(&line, dst_tok, (int)sizeof dst_tok) != 1 || !ip_parse(dst_tok, &dst)) {
        return reject("用法：ask <最终目的 IP> gw <网关 IP>/<前缀>");
    }
    if (take_token(&line, gw_word, (int)sizeof gw_word) != 1 || strcmp(gw_word, "gw") != 0) {
        return reject("用法：ask <最终目的 IP> gw <网关 IP>/<前缀>");
    }
    if (take_token(&line, gw_tok, (int)sizeof gw_tok) != 1 || !prefix_parse(gw_tok, &gw, &prefix) ||
        !rest_blank(line)) {
        return reject("网关写成 <IP>/<前缀>");
    }
    if (prefix <= 0 || prefix > 32) {
        return reject("前缀要在 1 到 32");
    }
    return ask_resolve(dst, gw, prefix);
}

static int ask_resolve(uint32_t dst, uint32_t gw, int prefix) {
    uint32_t who;
    uint32_t net;
    int remote = 0;
    uint8_t found[6];
    char dst_text[16];
    char who_text[16];
    char net_text[32];
    char mac_text[24];
    char host_text[16];

    who = arp_target(host_ip, prefix, dst, gw, &remote);
    net = host_ip & prefix_mask(prefix);
    ip_fmt(dst, dst_text, (int)sizeof dst_text);
    ip_fmt(who, who_text, (int)sizeof who_text);
    prefix_fmt(net, prefix, net_text, (int)sizeof net_text);
    ip_fmt(host_ip, host_text, (int)sizeof host_text);
    mac_fmt(host_mac, mac_text, (int)sizeof mac_text);
    printf("【ARP 请求】\n");
    if (remote) {
        printf("目的 %s 不在 %s 里，ARP 过不了路由器。\n", dst_text, net_text);
        printf("要解析的是默认网关 %s，不是最终目的。\n", who_text);
    } else {
        printf("目的 %s 在 %s 里，直接解析它。\n", dst_text, net_text);
    }
    if (arp_get(&cache, who, found)) {
        mac_fmt(found, mac_text, (int)sizeof mac_text);
        printf("缓存已有 %s → %s，不再广播。\n", who_text, mac_text);
        printf("结果：缓存命中，MAC %s\n", mac_text);
        return 0;
    }
    printf("缓存没有 %s。\n", who_text);
    printf("以太网目的 MAC 填 ff-ff-ff-ff-ff-ff，操作码 1（请求）。\n");
    printf("发送方 %s / %s\n", host_text, mac_text);
    printf("目标 %s / 00-00-00-00-00-00\n", who_text);
    pending = 1;
    pending_ip = who;
    printf("结果：ARP，目的 MAC ff-ff-ff-ff-ff-ff\n");
    return 0;
}

static int cmd_reply(char *line) {
    char ip_tok[32];
    char mac_tok[32];
    uint32_t ip;
    uint8_t mac[6];
    char ip_text[16];
    char mac_text[24];
    char host_text[16];
    char host_mac_text[24];

    if (!pending) {
        return reject("没有待响应的请求");
    }
    if (take_token(&line, ip_tok, (int)sizeof ip_tok) != 1 || !ip_parse(ip_tok, &ip)) {
        return reject("用法：reply <IP> <MAC>");
    }
    if (take_token(&line, mac_tok, (int)sizeof mac_tok) != 1 || !mac_parse(mac_tok, mac) || !rest_blank(line)) {
        return reject("MAC 写成 00-11-22-33-44-55");
    }
    if (ip != pending_ip) {
        return reject("响应的 IP 不是请求要解析的那个");
    }
    if (!arp_put(&cache, ip, mac)) {
        return reject("ARP 缓存没有空位");
    }
    pending = 0;
    ip_fmt(ip, ip_text, (int)sizeof ip_text);
    mac_fmt(mac, mac_text, (int)sizeof mac_text);
    ip_fmt(host_ip, host_text, (int)sizeof host_text);
    mac_fmt(host_mac, host_mac_text, (int)sizeof host_mac_text);
    printf("【ARP 响应】\n");
    printf("操作码 2（响应），以太网目的 MAC 是请求方的 %s，单播。\n", host_mac_text);
    printf("响应方从请求里学到 %s → %s。请求带着自己的映射，就是为了这一步。\n", host_text, host_mac_text);
    printf("请求方记下 %s → %s\n", ip_text, mac_text);
    printf("结果：%s 的 MAC 是 %s\n", ip_text, mac_text);
    return 0;
}

static int cmd_observer(char *line) {
    char tok[16];
    unsigned id;
    int n = 0;

    if (take_token(&line, tok, (int)sizeof tok) != 1 || !rest_blank(line)) {
        return reject("用法：observer <VLAN 号>");
    }
    if (sscanf(tok, "%u%n", &id, &n) != 1 || tok[n] != '\0' || id == 0 || id > 4094u) {
        return reject("VLAN 号要在 1 到 4094");
    }
    observer = (int)id;
    printf("【ARP 表】\n");
    printf("观察者在 VLAN %d。ARP 请求是广播，出不了这个 VLAN。\n", observer);
    printf("结果：观察者 VLAN %d\n", observer);
    return 0;
}

static int cmd_vlan(char *line) {
    char id_tok[16];
    char ip_tok[32];
    char mac_tok[32];
    unsigned id;
    int n = 0;
    uint32_t ip;
    uint8_t mac[6];
    char ip_text[16];
    char mac_text[24];

    if (take_token(&line, id_tok, (int)sizeof id_tok) != 1 || sscanf(id_tok, "%u%n", &id, &n) != 1 ||
        id_tok[n] != '\0' || id == 0 || id > 4094u) {
        return reject("用法：vlan <VLAN 号> <IP> <MAC>，编号 1 到 4094");
    }
    if (take_token(&line, ip_tok, (int)sizeof ip_tok) != 1 || !ip_parse(ip_tok, &ip)) {
        return reject("IP 不合法");
    }
    if (take_token(&line, mac_tok, (int)sizeof mac_tok) != 1 || !mac_parse(mac_tok, mac) || !rest_blank(line)) {
        return reject("MAC 写成 00-11-22-33-44-55");
    }
    if (!vlan_add(vlan_tab, &vlan_n, (int)id, ip, mac)) {
        return reject("VLAN 表最多 16 条，编号要在 1 到 4094");
    }
    ip_fmt(ip, ip_text, (int)sizeof ip_text);
    mac_fmt(mac, mac_text, (int)sizeof mac_text);
    printf("【VLAN】\n");
    printf("%s / %s 在 VLAN %u。\n", ip_text, mac_text, id);
    printf("结果：%s 在 VLAN %u\n", ip_text, id);
    return 0;
}

static int cmd_which(char *line) {
    char tok[32];
    uint32_t ip;
    int id;
    char ip_text[16];

    if (observer <= 0) {
        return reject("先用 observer 给出观察者的 VLAN");
    }
    if (take_token(&line, tok, (int)sizeof tok) != 1 || !ip_parse(tok, &ip) || !rest_blank(line)) {
        return reject("用法：which <IP>");
    }
    id = vlan_find(vlan_tab, vlan_n, ip);
    if (id == 0) {
        return reject("这台主机不在 VLAN 表里");
    }
    ip_fmt(ip, ip_text, (int)sizeof ip_text);
    printf("【ARP 表】\n");
    if (id == observer) {
        printf("%s 和观察者都在 VLAN %d，ARP 广播到得了。\n", ip_text, id);
        printf("结果：%s 在 VLAN %d，可以出现\n", ip_text, id);
        return 0;
    }
    printf("%s 在 VLAN %d，观察者在 VLAN %d。广播被 VLAN 隔开。\n", ip_text, id, observer);
    printf("结果：%s 在 VLAN %d，不会出现在 VLAN %d 的 ARP 表\n", ip_text, id, observer);
    return 0;
}

int run_arp(char *line) {
    char cmd[16];

    if (take_token(&line, cmd, (int)sizeof cmd) != 1) {
        return reject("缺少命令");
    }
    if (strcmp(cmd, "host") == 0) {
        return cmd_host(line);
    }
    if (strcmp(cmd, "ask") == 0) {
        return cmd_ask(line);
    }
    if (strcmp(cmd, "reply") == 0) {
        return cmd_reply(line);
    }
    if (strcmp(cmd, "observer") == 0) {
        return cmd_observer(line);
    }
    if (strcmp(cmd, "vlan") == 0) {
        return cmd_vlan(line);
    }
    if (strcmp(cmd, "which") == 0) {
        return cmd_which(line);
    }
    return reject("不认识的命令");
}
