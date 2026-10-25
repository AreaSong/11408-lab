#include "show_wifi.h"

#include <stdio.h>

static void print_addr(int index, const unsigned char mac[MAC_LEN], const char *role) {
    char text[MAC_TEXT];

    mac_fmt(mac, text);
    printf("地址 %d = %s（%s）\n", index, text, role);
}

static void show_sta_ap(const WifiFrame *w) {
    printf("这一帧是站点发给 AP。去往 AP = 1，来自 AP = 0。\n");
    printf("地址 1、2 是这一跳的接收方和发送方。站点发给 AP 时，地址 3 补上真正的目的 MAC。\n");
    printf("访问 Internet 时，这个目的 MAC 是默认网关，不是远端主机。MAC 只在本网段有意义。\n");
    print_addr(1, w->addr[0], "接收方，AP");
    print_addr(2, w->addr[1], "发送方，站点");
    print_addr(3, w->addr[2], "目的地址");
}

static void show_ap_sta(const WifiFrame *w) {
    printf("这一帧是 AP 发给站点。去往 AP = 0，来自 AP = 1。\n");
    printf("地址 1 是接收这帧的站点，地址 2 是 AP。地址 2 不是源主机。\n");
    printf("地址 3 补上真正的源。同一份数据要先到 AP，再由 AP 转给目的站点。\n");
    print_addr(1, w->addr[0], "接收方，目的站点");
    print_addr(2, w->addr[1], "发送方，AP");
    print_addr(3, w->addr[2], "源地址");
}

static void show_adhoc(const WifiFrame *w) {
    printf("自组网，没有 AP。去往 AP = 0，来自 AP = 0。\n");
    printf("地址 1 是目的站点，地址 2 是源站点，地址 3 是 BSSID。地址 4 不用。\n");
    print_addr(1, w->addr[0], "目的站点");
    print_addr(2, w->addr[1], "源站点");
    print_addr(3, w->addr[2], "BSSID");
}

static void show_wds(const WifiFrame *w) {
    printf("无线分布式系统里，AP 把帧转给另一个 AP。去往 AP = 1，来自 AP = 1。\n");
    printf("这时四个地址都要用。地址 4 只在这种帧里出现。\n");
    print_addr(1, w->addr[0], "接收 AP");
    print_addr(2, w->addr[1], "发送 AP");
    print_addr(3, w->addr[2], "目的地址");
    print_addr(4, w->addr[3], "源地址");
}

static void result_line(const WifiFrame *w) {
    int i;

    printf("结果：");
    for (i = 0; i < w->naddr; i++) {
        char text[MAC_TEXT];
        mac_fmt(w->addr[i], text);
        printf("%s%s", i ? "，" : "", text);
    }
    printf("\n");
}

void show_wifi(const WifiFrame *w) {
    printf("【802.11 地址】\n");
    if (w->scene == WIFI_STA_AP) {
        show_sta_ap(w);
    } else if (w->scene == WIFI_AP_STA) {
        show_ap_sta(w);
    } else if (w->scene == WIFI_ADHOC) {
        show_adhoc(w);
    } else {
        show_wds(w);
    }
    result_line(w);
}
