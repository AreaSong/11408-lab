#include "limit.h"

/* 最小的 n，使 2^n >= need。need 是序号空间，不是窗口本身。 */
static int cover(long long need) {
    int n = 0;
    long long space = 1;

    if (need < 1) {
        return 0;
    }
    while (space < need) {
        if (n >= 30) {
            return 0;
        }
        n++;
        space <<= 1;
    }
    return n;
}

int limit_space(int bits) {
    if (bits < 1 || bits > 16) {
        return 0;
    }
    return 1 << bits;
}

int limit_max_gbn(int bits) {
    int space = limit_space(bits);

    if (space == 0) {
        return 0;
    }
    return space - 1;
}

int limit_max_sr_equal(int bits) {
    if (limit_space(bits) == 0) {
        return 0;
    }
    return 1 << (bits - 1);
}

int limit_max_sr_recv(int bits, int ws) {
    int space = limit_space(bits);
    int by_sum;

    if (space == 0 || ws < 1 || ws >= space) {
        return 0;
    }
    by_sum = space - ws;
    /* 王道还要求接收窗口不超过发送窗口，两条里更紧的那条才是上限。 */
    return by_sum < ws ? by_sum : ws;
}

int limit_bits_gbn(int window) {
    if (window < 1) {
        return 0;
    }
    return cover((long long)window + 1);
}

int limit_bits_sr_equal(int window) {
    if (window < 1) {
        return 0;
    }
    return cover((long long)window * 2);
}

int limit_bits_slide(int window) {
    int gbn = limit_bits_gbn(window);
    int sr = limit_bits_sr_equal(window);

    if (gbn == 0 || sr == 0) {
        return 0;
    }
    return gbn > sr ? gbn : sr;
}
