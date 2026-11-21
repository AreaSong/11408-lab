#include "user/user_net.h"

#include "net/net_arp.h"
#include "net/net_http.h"
#include "net/net_mail.h"

#include <stdio.h>

static void ip_fmt(unsigned ip, char *buf, int cap) {
    snprintf(buf, (size_t)cap, "%u.%u.%u.%u", (ip >> 24) & 255u, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u);
}

int user_ping_line(char *out, int cap) {
    unsigned who = 0;
    char ip[20];
    int n;
    if (out == NULL || net_arp_who(0x0a000002u, 0x0a000001u, 24, 0x0a000001u, &who) != 0 || who != 0x0a000002u) {
        return -1;
    }
    ip_fmt(who, ip, (int)sizeof ip);
    n = snprintf(out, (size_t)cap, "who=%s same=1", ip);
    return n > 0 && n < cap ? 0 : -1;
}

int user_curl_line(char *out, int cap) {
    int rtt = net_http_rtts(0, 0, 1, 0);
    int n;
    if (out == NULL || rtt != 2) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "rtt=%d mode=nonpersist", rtt);
    return n > 0 && n < cap ? 0 : -1;
}

int user_mail_tool(char *out, int cap) {
    return net_mail_line("2012-40", out, cap);
}
