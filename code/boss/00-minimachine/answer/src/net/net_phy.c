#include "net/net_phy.h"

#include <stdio.h>
#include <string.h>

static int log2_pow(int states) {
    int bits = 0;
    int v = states;
    if (states < 2 || (states & (states - 1)) != 0) {
        return -1;
    }
    while (v > 1) {
        v >>= 1;
        bits++;
    }
    return bits;
}

static int log2_near(long long x, int *out) {
    int k;
    for (k = 1; k < 30; k++) {
        long long pow = 1LL << k;
        long long big = pow > x ? pow : x;
        long long small = pow > x ? x : pow;
        if (big * 100 <= small * 103) {
            *out = k;
            return pow == x ? 0 : 1;
        }
    }
    return -1;
}

static int snr_from_db(int db, long long *snr) {
    int i;
    long long v = 1;
    if (db < 0 || db % 10 != 0 || db > 60) {
        return -1;
    }
    for (i = 0; i < db / 10; i++) {
        v *= 10;
    }
    *snr = v;
    return 0;
}

int net_nyquist(long long hz, int states, long long *bps) {
    int bits = log2_pow(states);
    if (bps == NULL || hz <= 0 || bits < 0) {
        return -1;
    }
    *bps = 2 * hz * bits;
    return 0;
}

int net_shannon(long long hz, int db, int percent, long long *bps, int *approx) {
    long long snr = 0;
    int lg = 0;
    int snapped;
    if (bps == NULL || approx == NULL || hz <= 0 || percent < 1 || percent > 100) {
        return -1;
    }
    if (snr_from_db(db, &snr) != 0) {
        return -1;
    }
    snapped = log2_near(1 + snr, &lg);
    if (snapped < 0) {
        return -1;
    }
    *approx = snapped;
    *bps = hz * lg * percent / 100;
    return 0;
}

int net_qam(long long hz, long long bps, char *name, int cap) {
    long long baud;
    long long bits;
    if (name == NULL || hz <= 0 || bps <= 0) {
        return -1;
    }
    baud = 2 * hz;
    if (bps % baud != 0) {
        return -1;
    }
    bits = bps / baud;
    if (bits < 1 || bits > 16) {
        return -1;
    }
    snprintf(name, (size_t)cap, "QAM-%lld", 1LL << bits);
    return 0;
}

int net_min_states(int db) {
    long long snr = 0;
    int v;
    if (snr_from_db(db, &snr) != 0) {
        return -1;
    }
    for (v = 2; v < 100000; v++) {
        if ((long long)v * v >= 1 + snr) {
            return v;
        }
    }
    return -1;
}

int net_manch(const char *bits, char *out, int cap) {
    int n;
    int i;
    int o = 0;
    if (bits == NULL || out == NULL) {
        return -1;
    }
    n = (int)strlen(bits);
    if (o + 2 * n >= cap) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        /* 高到低是 1，低到高是 0。曼彻斯特每个比特中间都有一次跳变。 */
        if (bits[i] == '1') {
            out[o++] = '1';
            out[o++] = '0';
        } else if (bits[i] == '0') {
            out[o++] = '0';
            out[o++] = '1';
        } else {
            return -1;
        }
    }
    out[o] = '\0';
    return 0;
}

int net_manch_baud(long long bps, long long *baud) {
    if (baud == NULL || bps <= 0) {
        return -1;
    }
    *baud = bps * 2;
    return 0;
}
