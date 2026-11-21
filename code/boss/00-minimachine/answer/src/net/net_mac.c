#include "net/net_mac.h"

#include <string.h>

long long net_min_bits(long long bps, long long meters, long long mps) {
    if (bps <= 0 || meters < 0 || mps <= 0) {
        return -1;
    }
    /* 先乘再除。最短帧要盖住往返传播，不是单程。 */
    return bps * 2 * meters / mps;
}

long long net_delta_m(long long bits, long long bps, long long mps) {
    if (bits < 0 || bps <= 0 || mps <= 0) {
        return -1;
    }
    return bits * mps / bps / 2;
}

long long net_reach_m(long long frame_bits, long long bps, long long dev_ns, long long m_per_us) {
    long long budget;
    if (frame_bits <= 0 || bps <= 0 || dev_ns < 0 || m_per_us <= 0) {
        return -1;
    }
    budget = frame_bits * 1000000000LL / bps / 2;
    if (budget < dev_ns) {
        return -1;
    }
    /* 集线器再生在去和回各发生一次，所以从单程预算里扣，不从往返里扣两次。 */
    return (budget - dev_ns) * m_per_us / 1000;
}

long long net_one_way_ns(long long frame_bits, long long bps) {
    if (frame_bits <= 0 || bps <= 0) {
        return -1;
    }
    return frame_bits * 1000000000LL / bps / 2;
}

long long net_backoff_ns(int collisions, long long slot_ns) {
    int k;
    int rmax;
    if (collisions < 1 || slot_ns <= 0) {
        return -1;
    }
    /* 连续 16 次冲突就放弃这一帧。k 到 10 就不再加倍。 */
    if (collisions >= 16) {
        return -1;
    }
    k = collisions < 10 ? collisions : 10;
    rmax = (1 << k) - 1;
    return (long long)rmax * slot_ns;
}

int net_cdma_decode(const int *chip, int nchip, const int *recv, int nrecv, char *out, int cap) {
    int bits;
    int i;
    int k;
    if (chip == NULL || recv == NULL || out == NULL || nchip < 1 || nrecv % nchip != 0) {
        return -1;
    }
    bits = nrecv / nchip;
    if (bits + 1 > cap) {
        return -1;
    }
    for (i = 0; i < bits; i++) {
        int sum = 0;
        for (k = 0; k < nchip; k++) {
            sum += chip[k] * recv[i * nchip + k];
        }
        if (sum > 0) {
            out[i] = '1';
        } else if (sum < 0) {
            out[i] = '0';
        } else {
            out[i] = 'x';
        }
    }
    out[bits] = '\0';
    return 0;
}

int net_cdma_2014(char *out, int cap) {
    int chip[4] = {1, 1, 1, 1};
    int recv[12] = {2, 0, 2, 0, 0, -2, 0, -2, 0, 2, 0, 2};
    return net_cdma_decode(chip, 4, recv, 12, out, cap);
}
