#include "net/net_win.h"

#include <stdio.h>
#include <string.h>

static int known(const char *proto) {
    return proto != NULL && (strcmp(proto, "stop") == 0 || strcmp(proto, "gbn") == 0 ||
                             strcmp(proto, "sr") == 0 || strcmp(proto, "slide") == 0);
}

static int ceil_div(long long a, long long b) {
    if (a <= 0 || b <= 0) {
        return -1;
    }
    return (int)((a + b - 1) / b);
}

static int one_need(long long bits, long long ack, long long bps, long long prop, int util) {
    long long td;
    long long ta;
    long long period;
    if (bits <= 0 || bps <= 0 || prop < 0 || util < 1 || util > 100) {
        return -1;
    }
    if ((bits * 1000000000LL) % bps != 0) {
        return -1;
    }
    td = bits * 1000000000LL / bps;
    if (ack < 0) {
        ta = td;
    } else if (ack == 0) {
        ta = 0;
    } else if ((ack * 1000000000LL) % bps != 0) {
        return -1;
    } else {
        ta = ack * 1000000000LL / bps;
    }
    period = td + ta + 2 * prop;
    if (util == 100) {
        return ceil_div(period, td);
    }
    return ceil_div(util * period, 100 * td);
}

static int seq_bits(const char *proto, int window) {
    int n;
    if (window < 1) {
        return -1;
    }
    if (strcmp(proto, "stop") == 0) {
        return window == 1 ? 1 : -1;
    }
    for (n = 1; n <= 16; n++) {
        /* GBN 要空一个序号，最大窗口 2^n-1。SR 收发窗口相等时各是 2^(n-1)。 */
        int maxw = strcmp(proto, "sr") == 0 ? (1 << (n - 1)) : ((1 << n) - 1);
        if (maxw >= window) {
            return n;
        }
    }
    return -1;
}

int net_need(const char *proto, long long bits_lo, long long bits_hi, int ack_same, long long bps,
             long long prop_ns, int util_pct, int *window, int *bits) {
    long long ack_lo = ack_same ? -1 : 0;
    long long ack_hi = ack_same ? -1 : 0;
    int w1;
    int w2;
    int best;
    if (!known(proto) || window == NULL || bits == NULL || bits_lo <= 0 || bits_hi < bits_lo) {
        return -1;
    }
    w1 = one_need(bits_lo, ack_lo, bps, prop_ns, util_pct);
    w2 = one_need(bits_hi, ack_hi, bps, prop_ns, util_pct);
    if (w1 < 0 || w2 < 0) {
        return -1;
    }
    /* 帧越短，确认回来之前要连发的帧越多，序号按更大的那一档取。 */
    best = w1 > w2 ? w1 : w2;
    *window = best;
    *bits = seq_bits(proto, best);
    return *bits < 0 ? -1 : 0;
}

int net_rate(const char *proto, int window, long long bits, long long ack_bits, long long bps,
             long long prop_ns, long long *out) {
    long long td;
    long long ta = 0;
    long long period;
    if (!known(proto) || out == NULL || window < 1 || bits <= 0 || ack_bits < 0 || bps <= 0 || prop_ns < 0) {
        return -1;
    }
    if ((bits * 1000000000LL) % bps != 0) {
        return -1;
    }
    td = bits * 1000000000LL / bps;
    if (ack_bits > 0) {
        if ((ack_bits * 1000000000LL) % bps != 0) {
            return -1;
        }
        ta = ack_bits * 1000000000LL / bps;
    }
    period = td + ta + 2 * prop_ns;
    if (period <= 0) {
        return -1;
    }
    *out = (long long)window * bits * 1000000000LL / period;
    if (*out > bps) {
        *out = bps;
    }
    return 0;
}

int net_limit_eq(const char *proto, int nbits, int *send_w, int *recv_w) {
    if (send_w == NULL || recv_w == NULL || nbits < 1 || nbits > 16 || proto == NULL) {
        return -1;
    }
    if (strcmp(proto, "stop") == 0) {
        *send_w = 1;
        *recv_w = 1;
        return 0;
    }
    if (strcmp(proto, "gbn") == 0) {
        *send_w = (1 << nbits) - 1;
        *recv_w = 1;
        return 0;
    }
    if (strcmp(proto, "sr") == 0) {
        *send_w = 1 << (nbits - 1);
        *recv_w = *send_w;
        return 0;
    }
    return -1;
}

int net_room(int nbits, int send_w, int *recv_w) {
    int space;
    if (recv_w == NULL || nbits < 1 || nbits > 16 || send_w < 1) {
        return -1;
    }
    space = 1 << nbits;
    /* SR：发送窗口和接收窗口之和不能超过序号空间，否则新旧帧分不清。 */
    if (send_w >= space) {
        return -1;
    }
    *recv_w = space - send_w;
    return 0;
}

int net_cmp_u(int nbits, char *dst, int cap) {
    int stop_w;
    int stop_r;
    int sr_w;
    int sr_r;
    int gbn_w;
    int gbn_r;
    if (dst == NULL || cap < 16) {
        return -1;
    }
    if (net_limit_eq("stop", nbits, &stop_w, &stop_r) != 0 || net_limit_eq("sr", nbits, &sr_w, &sr_r) != 0 ||
        net_limit_eq("gbn", nbits, &gbn_w, &gbn_r) != 0) {
        return -1;
    }
    /* 同样的帧长下利用率随窗口变大，直到封顶。1 ≤ 2^(n-1) ≤ 2^n-1。 */
    if (!(stop_w <= sr_w && sr_w <= gbn_w)) {
        return -1;
    }
    snprintf(dst, (size_t)cap, "U1 <= U3 <= U2");
    return 0;
}
