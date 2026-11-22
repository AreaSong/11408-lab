#include "dhcp.h"

#include "addr.h"

#include <stdio.h>

static void fill_step(DhcpStep *step, const char *name, const char *src, const char *dst, int sport, int dport,
                      const char *client, const char *server) {
    snprintf(step->name, sizeof step->name, "%s", name);
    /* 首次获取按谢希仁：四步都广播。ACK 之前客户不能用分配到的地址。 */
    snprintf(step->how, sizeof step->how, "广播");
    snprintf(step->src, sizeof step->src, "%s", src);
    snprintf(step->dst, sizeof step->dst, "%s", dst);
    step->sport = sport;
    step->dport = dport;
    snprintf(step->client, sizeof step->client, "%s", client);
    snprintf(step->server, sizeof step->server, "%s", server);
}

static int host_ip(uint32_t ip) {
    return ip != 0 && ip != 0xffffffffu;
}

int dhcp_initial(uint32_t server, uint32_t offer, DhcpStep step[DHCP_STEPS]) {
    char srv[32];
    char off[32];
    char offered[80];
    char bound[80];

    if (step == NULL || !host_ip(server) || !host_ip(offer) || server == offer) {
        return 0;
    }
    ip_fmt(server, srv, (int)sizeof srv);
    ip_fmt(offer, off, (int)sizeof off);
    snprintf(offered, sizeof offered, "已提供 %s，这个地址先留着", off);
    snprintf(bound, sizeof bound, "已绑定，这时才能使用 %s", off);
    fill_step(&step[0], "DISCOVER", "0.0.0.0", "255.255.255.255", 68, 67, "初始化，发出后进入选择",
              "监听，收到后从地址池取地址，尚未绑定");
    fill_step(&step[1], "OFFER", srv, "255.255.255.255", 67, 68, "选择，可以收到多份提供，还不能用这个地址", offered);
    fill_step(&step[2], "REQUEST", "0.0.0.0", "255.255.255.255", 68, 67, "请求，用广播告诉其他服务器自己没选它们",
              "被选中的准备确认，其余服务器收回提供");
    fill_step(&step[3], "ACK", srv, "255.255.255.255", 67, 68, bound, "已绑定，租约记在这台客户机上");
    return 1;
}

int dhcp_same_lan_hears_discover(int same_lan) {
    /* 255.255.255.255 是受限广播，路由器不转发到别的网络。 */
    return same_lan ? 1 : 0;
}
