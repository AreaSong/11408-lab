#include "net/net_delay.h"

#include <stddef.h>

static int exact_ns(long long bits, long long bps, long long *ns) {
    if (bits < 0 || bps <= 0 || (bits * 1000000000LL) % bps != 0) {
        return -1;
    }
    *ns = bits * 1000000000LL / bps;
    return 0;
}

int net_link(long long bits, long long bps, long long meters, long long mps, long long proc_ns,
             long long queue_ns, NetDelay *out) {
    long long tx;
    long long prop;
    if (out == NULL || meters < 0 || mps <= 0 || proc_ns < 0 || queue_ns < 0) {
        return -1;
    }
    if (exact_ns(bits, bps, &tx) != 0 || (meters * 1000000000LL) % mps != 0) {
        return -1;
    }
    prop = meters * 1000000000LL / mps;
    if ((prop * bps) % 1000000000LL != 0) {
        return -1;
    }
    out->tx_ns = tx;
    out->prop_ns = prop;
    out->total_ns = tx + prop + proc_ns + queue_ns;
    out->rtt_ns = prop * 2;
    out->bdp_bit = prop * bps / 1000000000LL;
    return 0;
}

int net_util_pct(long long data_bits, long long ack_bits, long long bps, long long prop_ns, int *pct) {
    long long td;
    long long ta;
    long long period;
    /* 停等的分母含确认帧。2020-36 把确认帧去掉会得到约 66.7%，那是干扰项。 */
    if (pct == NULL || prop_ns < 0 || exact_ns(data_bits, bps, &td) != 0 || exact_ns(ack_bits, bps, &ta) != 0) {
        return -1;
    }
    period = td + ta + 2 * prop_ns;
    if (period <= 0 || (td * 100) % period != 0) {
        return -1;
    }
    *pct = (int)(td * 100 / period);
    return 0;
}

int net_goodput(long long data_bits, long long header_bits, long long bps, long long *out) {
    long long total = data_bits + header_bits;
    if (out == NULL || data_bits < 0 || header_bits < 0 || total <= 0 || bps <= 0) {
        return -1;
    }
    if ((bps * data_bits) % total != 0) {
        return -1;
    }
    *out = bps * data_bits / total;
    return 0;
}

int net_eff(long long payload, long long frame, long long ack, long long bps, long long prop_ns, long long *out) {
    long long td;
    long long ta;
    long long period;
    if (out == NULL || payload < 0 || prop_ns < 0 || exact_ns(frame, bps, &td) != 0 ||
        exact_ns(ack, bps, &ta) != 0) {
        return -1;
    }
    period = td + ta + 2 * prop_ns;
    if (period <= 0) {
        return -1;
    }
    *out = payload * 1000000000LL / period;
    return 0;
}
