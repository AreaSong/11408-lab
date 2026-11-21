#include "net/net_tcp.h"

#include <stdio.h>
#include <string.h>

int net_tcp_hs2(unsigned cisn, unsigned sisn, unsigned *seq, unsigned *ack) {
    if (seq == NULL || ack == NULL) {
        return -1;
    }
    /* 带 SYN 的段按王道不携带数据，SYN 自己占 1 个序号。 */
    *seq = sisn;
    *ack = cisn + 1u;
    return 0;
}

unsigned net_tcp_hs3(unsigned sisn) { return sisn + 1u; }

unsigned net_tcp_payload(unsigned syn_seq, unsigned fin_seq) {
    /* SYN 占掉 syn_seq，FIN 占掉 fin_seq，中间才是数据。 */
    return fin_seq - syn_seq - 1u;
}

unsigned net_tcp_cumul(unsigned base, const int *len, const int *got, int n) {
    unsigned at = base;
    int i;
    for (i = 0; i < n; i++) {
        if (!got[i]) {
            return at;
        }
        at += (unsigned)len[i];
    }
    return at;
}

void net_tcp_reply(unsigned seq, unsigned ack, unsigned len, unsigned *oseq, unsigned *oack) {
    /* 纯数据段的回复序号就是对方的确认号，不要再加 1。确认号是最后一字节加 1。 */
    *oseq = ack;
    *oack = seq + len;
}

const char *net_tcp_active(int step) {
    static const char *name[] = {"FIN_WAIT_1", "FIN_WAIT_2", "FIN_WAIT_2", "TIME_WAIT"};
    if (step < 0 || step > 3) {
        return NULL;
    }
    return name[step];
}

static int line_hs(char *out, int cap) {
    unsigned seq = 0;
    unsigned ack = 0;
    int n;
    if (net_tcp_hs2(11220u, 11221u, &seq, &ack) != 0) {
        return -1;
    }
    /* 11221 是选项里一个合法的服务器序号，不是从 11220 推出来的。 */
    n = snprintf(out, (size_t)cap, "syn=1 ack=1 seq=%u ackno=%u letter=C", seq, ack);
    return n > 0 && n < cap ? 0 : -1;
}

int net_tcp_line(const char *id, char *out, int cap) {
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2011-39") == 0) {
        return line_hs(out, cap);
    }
    if (strcmp(id, "2019-39") == 0) {
        n = snprintf(out, (size_t)cap, "ackno=%u letter=D", net_tcp_hs3(2046u));
    } else if (strcmp(id, "2021-38") == 0) {
        n = snprintf(out, (size_t)cap, "state=%s letter=B", net_tcp_active(3));
    } else if (strcmp(id, "2009-38") == 0) {
        int len[] = {300, 500};
        int got[] = {1, 1};
        n = snprintf(out, (size_t)cap, "ackno=%u letter=D", net_tcp_cumul(200u, len, got, 2));
    } else if (strcmp(id, "2011-40") == 0) {
        int len[] = {300, 400, 500};
        int got[] = {1, 0, 1};
        n = snprintf(out, (size_t)cap, "ackno=%u letter=B", net_tcp_cumul(200u, len, got, 3));
    } else if (strcmp(id, "2013-39") == 0) {
        unsigned seq = 0;
        unsigned ack = 0;
        net_tcp_reply(1913u, 2046u, 100u, &seq, &ack);
        n = snprintf(out, (size_t)cap, "seq=%u ackno=%u letter=B", seq, ack);
    } else if (strcmp(id, "2020-39") == 0) {
        n = snprintf(out, (size_t)cap, "bytes=%u letter=C", net_tcp_payload(1000u, 5001u));
    } else if (strcmp(id, "both") == 0) {
        /* 在 FIN_WAIT_1 收到的是对方的 FIN，不是自己的 ACK，进入 CLOSING。 */
        n = snprintf(out, (size_t)cap, "state=CLOSING");
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
