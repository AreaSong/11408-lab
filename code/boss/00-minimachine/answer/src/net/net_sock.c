#include "net/net_sock.h"

#include "net/net_cong.h"
#include "net/net_dns.h"
#include "net/net_http.h"
#include "net/net_mail.h"
#include "net/net_tcp.h"
#include "net/net_udp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_MAX 8
#define APP_BUF 128

typedef struct {
    int used;
    int type;
    int state;
    int port;
    int peer_id;
    uint32_t peer_ip;
    int peer_port;
    char rx[APP_BUF];
    int rx_len;
} AppSock;

struct AppNs {
    AppSock s[APP_MAX];
    int next_port;
};

AppNs *net_app_new(void) {
    AppNs *a = calloc(1, sizeof *a);
    if (a != NULL) {
        a->next_port = 49152;
    }
    return a;
}

void net_app_free(AppNs *a) { free(a); }

void net_app_clear(AppNs *a) {
    int next;
    if (a == NULL) {
        return;
    }
    next = a->next_port;
    memset(a, 0, sizeof *a);
    a->next_port = next > 0 ? next : 49152;
}

static AppSock *slot(AppNs *a, int id) {
    if (a == NULL || id < 0 || id >= APP_MAX || !a->s[id].used) {
        return NULL;
    }
    return &a->s[id];
}

static int find_port(const AppNs *a, int type, int port) {
    int i;
    for (i = 0; i < APP_MAX; i++) {
        if (a->s[i].used && a->s[i].type == type && a->s[i].port == port) {
            return i;
        }
    }
    return -1;
}

int net_app_socket(AppNs *a, int type, int port) {
    int id = -1;
    int i;
    int listen;
    if (a == NULL || (type != 1 && type != 2) || port < 0 || port > 65535) {
        return -1;
    }
    for (i = 0; i < APP_MAX; i++) {
        if (!a->s[i].used) {
            id = i;
            break;
        }
    }
    if (id < 0) {
        return -1;
    }
    listen = type == 2 && port != 0;
    if (port == 0) {
        if (a->next_port > 65535) {
            return -1;
        }
        port = a->next_port++;
    } else if (find_port(a, type, port) >= 0) {
        return -1;
    }
    memset(&a->s[id], 0, sizeof a->s[id]);
    a->s[id].used = 1;
    a->s[id].type = type;
    a->s[id].port = port;
    a->s[id].peer_id = -1;
    a->s[id].state = listen || type == 1 ? 1 : 0;
    return id;
}

int net_app_connect(AppNs *a, int id, uint32_t ip, int port) {
    AppSock *s = slot(a, id);
    int dest;
    if (s == NULL || port <= 0 || port > 65535) {
        return -1;
    }
    dest = find_port(a, s->type, port);
    if (dest < 0 || dest == id) {
        return -1;
    }
    if (s->type == 2 && a->s[dest].state != 1) {
        return -1;
    }
    s->peer_ip = ip;
    s->peer_port = port;
    s->peer_id = dest;
    if (s->type == 2) {
        /* 同一台机器上的环回，三次握手立刻完成。 */
        s->state = 2;
        a->s[dest].state = 2;
        a->s[dest].peer_id = id;
        a->s[dest].peer_port = s->port;
        a->s[dest].peer_ip = 0x7f000001u;
    }
    return 0;
}

int net_app_send(AppNs *a, int id, const void *buf, int n) {
    AppSock *s = slot(a, id);
    AppSock *d;
    if (s == NULL || buf == NULL || n < 0 || n > APP_BUF || s->peer_id < 0) {
        return -1;
    }
    d = slot(a, s->peer_id);
    if (d == NULL || d->rx_len + n > APP_BUF) {
        return -1;
    }
    memcpy(d->rx + d->rx_len, buf, (size_t)n);
    d->rx_len += n;
    return n;
}

int net_app_recv(AppNs *a, int id, void *buf, int cap) {
    AppSock *s = slot(a, id);
    int n;
    if (s == NULL || buf == NULL || cap < 0) {
        return -1;
    }
    n = s->rx_len < cap ? s->rx_len : cap;
    if (n > 0) {
        memcpy(buf, s->rx, (size_t)n);
    }
    if (n < s->rx_len) {
        memmove(s->rx, s->rx + n, (size_t)(s->rx_len - n));
        s->rx_len -= n;
    } else {
        s->rx_len = 0;
    }
    return n;
}

static int add_line(char *out, int cap, int *used, const char *line) {
    int n = snprintf(out + *used, (size_t)cap - (size_t)*used, "%s\n", line);
    if (n < 0 || *used + n >= cap) {
        return -1;
    }
    *used += n;
    return 0;
}

static int fill_open(char *out, int cap, int *used) {
    char line[96];
    unsigned seq = 0;
    unsigned ack = 0;
    if (net_tcp_hs2(1000u, 5000u, &seq, &ack) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "open c=SYN_SENT s=LISTEN seq=1000");
    if (add_line(out, cap, used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "open c=SYN_SENT s=SYN_RCVD seq=%u ack=%u", seq, ack);
    if (add_line(out, cap, used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "open c=ESTABLISHED s=ESTABLISHED seq=%u ack=%u", 1000u + 1u,
             net_tcp_hs3(5000u));
    return add_line(out, cap, used, line);
}

static int fill_cwnd(char *out, int cap, int *used) {
    int seq[8];
    int thresh = 0;
    int fast_th = 0;
    int fast_cw = 0;
    char line[96];
    int n;
    if (net_cong_trace(16, 4, &thresh, seq, 8) != 5 || net_cong_fast(16, &fast_th, &fast_cw) != 0) {
        return -1;
    }
    n = snprintf(line, sizeof line, "cwnd=%d,%d,%d,%d,%d thresh=%d", seq[0], seq[1], seq[2], seq[3], seq[4],
                 thresh);
    if (n < 0 || n >= (int)sizeof line || add_line(out, cap, used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "fast cwnd=%d thresh=%d", fast_cw, fast_th);
    return add_line(out, cap, used, line);
}

static int fill_sock(char *out, int cap, int *used) {
    AppNs *box = net_app_new();
    char line[64];
    char got[8];
    int srv = net_app_socket(box, 1, 9000);
    int cli = net_app_socket(box, 1, 0);
    int n = -1;
    if (box != NULL && srv >= 0 && cli >= 0 && net_app_connect(box, cli, 0x7f000001u, 9000) == 0 &&
        net_app_send(box, cli, "hi", 2) == 2 && net_app_recv(box, srv, got, (int)sizeof got) == 2) {
        got[2] = '\0';
        snprintf(line, sizeof line, "sock echo=%s", got);
        n = add_line(out, cap, used, line);
    }
    net_app_free(box);
    return n;
}

static int fill_app(char *out, int cap, int *used) {
    char line[160];
    char body[160];
    int host = 0;
    int local = 0;
    int total = 0;
    int rtts = 0;
    int cwnd = 0;
    static const unsigned char hi[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
    if (net_dns_count(1, 0, 3, &host, &local, &total) != 0 || net_http_slow(1, 3, &rtts, &cwnd) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "dns host=%d local=%d total=%d", host, local, total);
    if (add_line(out, cap, used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "page rtt=%d ms=%d", rtts, rtts * 10);
    if (add_line(out, cap, used, line) != 0) {
        return -1;
    }
    if (net_b64_encode(hi, 6, body, (int)sizeof body) < 0) {
        return -1;
    }
    snprintf(line, sizeof line, "smtp HELO=250 DATA=354 dot=..");
    if (add_line(out, cap, used, line) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "b64=%s", body);
    if (add_line(out, cap, used, line) != 0) {
        return -1;
    }
    if (add_line(out, cap, used, "pop3 RETR=+OK") != 0) {
        return -1;
    }
    return fill_sock(out, cap, used);
}

int net_session_line(char *out, int cap) {
    int used = 0;
    char sum[16];
    char line[64];
    if (out == NULL || cap < 64) {
        return -1;
    }
    out[0] = '\0';
    snprintf(sum, sizeof sum, "%04x", net_udp_fold2(0xb9b6u, 0x65c5u));
    snprintf(line, sizeof line, "udp sum=%s", sum);
    if (add_line(out, cap, &used, line) != 0 || fill_open(out, cap, &used) != 0) {
        return -1;
    }
    snprintf(line, sizeof line, "close c=%s s=CLOSED", net_tcp_active(3));
    if (add_line(out, cap, &used, line) != 0 || fill_cwnd(out, cap, &used) != 0 ||
        fill_app(out, cap, &used) != 0) {
        return -1;
    }
    return 0;
}
