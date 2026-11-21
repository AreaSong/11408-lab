#include "net/net_cong.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int mss;
    int cwnd;
    int thresh;
    int rwnd;
    int buf;
    int used;
} NetWin;

static int win_rwnd(const NetWin *w) {
    int left;
    if (w->buf > 0) {
        left = w->buf - w->used;
        return left > 0 ? left : 0;
    }
    return w->rwnd;
}

static int win_swnd(const NetWin *w) {
    int r = win_rwnd(w);
    if (r < 0 || w->cwnd < r) {
        return w->cwnd;
    }
    return r;
}

static void win_timeout(NetWin *w) {
    int half = (w->cwnd / w->mss) / 2;
    if (half < 2) {
        half = 2;
    }
    /* 门限用发生时的旧窗口。先改 cwnd 再除，会小一倍。 */
    w->thresh = half * w->mss;
    w->cwnd = w->mss;
    w->used = 0;
}

static int win_round(NetWin *w) {
    int sent = win_swnd(w) / w->mss * w->mss;
    int sum;
    if (sent <= 0) {
        return 0;
    }
    if (w->buf > 0) {
        w->used += sent;
    }
    if (w->cwnd < w->thresh) {
        sum = w->cwnd + sent;
        w->cwnd = sum > w->thresh ? w->thresh : sum;
    } else {
        w->cwnd += w->mss;
    }
    return 1;
}

int net_cong_run(int cwnd, int thresh, int rwnd, int buf, int timeout, int rounds, int *out_cwnd,
                 int *out_swnd, int *out_rwnd) {
    NetWin w;
    int i;
    if (cwnd <= 0 || rounds < 0 || out_cwnd == NULL || out_swnd == NULL || out_rwnd == NULL) {
        return -1;
    }
    memset(&w, 0, sizeof w);
    w.mss = 1;
    w.cwnd = cwnd;
    w.thresh = thresh > 0 ? thresh : cwnd;
    w.rwnd = rwnd;
    w.buf = buf;
    if (timeout) {
        win_timeout(&w);
    }
    for (i = 0; i < rounds; i++) {
        if (!win_round(&w)) {
            return -1;
        }
    }
    *out_cwnd = w.cwnd;
    *out_swnd = win_swnd(&w);
    *out_rwnd = win_rwnd(&w);
    return 0;
}

int net_cong_trace(int kb0, int rounds, int *thresh, int *out, int cap) {
    NetWin w;
    int i;
    if (kb0 <= 0 || rounds < 0 || rounds + 1 > cap || thresh == NULL || out == NULL) {
        return -1;
    }
    memset(&w, 0, sizeof w);
    w.mss = 1;
    w.cwnd = kb0;
    w.thresh = kb0;
    w.rwnd = -1;
    win_timeout(&w);
    *thresh = w.thresh;
    out[0] = w.cwnd;
    for (i = 0; i < rounds; i++) {
        if (!win_round(&w)) {
            return -1;
        }
        out[i + 1] = w.cwnd;
    }
    return rounds + 1;
}

int net_cong_back(int kb) {
    NetWin w;
    int n = 0;
    memset(&w, 0, sizeof w);
    w.mss = 1;
    w.cwnd = kb;
    w.rwnd = -1;
    win_timeout(&w);
    while (w.cwnd < kb && n < 64) {
        if (!win_round(&w)) {
            return -1;
        }
        n++;
    }
    return w.cwnd == kb ? n : -1;
}

int net_cong_span(int from, int to, int slow) {
    int n = 0;
    int c = from;
    if (from <= 0 || to < from) {
        return -1;
    }
    if (!slow) {
        return to - from;
    }
    while (c < to) {
        c *= 2;
        n++;
    }
    return c == to ? n : -1;
}

int net_cong_fast(int kb, int *thresh, int *cwnd) {
    int half = kb / 2;
    if (kb < 2 || thresh == NULL || cwnd == NULL) {
        return -1;
    }
    if (half < 2) {
        half = 2;
    }
    /* 王道的快恢复把 cwnd 设成新门限，不加 3 个 MSS。 */
    *thresh = half;
    *cwnd = half;
    return 0;
}

int net_cong_2016(int *rwnd, int *cwnd, int *swnd, unsigned *seq, int *rounds, int *kbps_x100) {
    int win = 1;
    int thresh = 64;
    int used = 0;
    int budget = 1;
    int got = 0;
    int acks = 0;
    int rnd = 0;
    int room;
    if (rwnd == NULL || cwnd == NULL || swnd == NULL || seq == NULL || rounds == NULL || kbps_x100 == NULL) {
        return -1;
    }
    while (used < 20) {
        used++;
        acks++;
        got++;
        if (win < thresh) {
            win++;
        }
        if (acks == 8) {
            *rwnd = 20 - used;
            *cwnd = win;
            *swnd = win < *rwnd ? win : *rwnd;
        }
        if (got == budget) {
            rnd++;
            got = 0;
            room = 20 - used;
            budget = win < room ? win : room;
            if (budget < 0) {
                budget = 0;
            }
        }
        if (budget == 0) {
            break;
        }
    }
    if (used != 20 || acks < 8) {
        return -1;
    }
    *seq = 101u + 20u * 1024u;
    *rounds = rnd;
    /* 20 KiB/s = 20480 B/s。k = 10^3，再乘 8 bit，是 163.84 kbit/s。 */
    *kbps_x100 = (20 * 1024 * 8) / 10;
    return 0;
}

int net_cong_ftp(int ackno, int *mss) {
    int cwnd = 1;
    int thresh = 4;
    int budget = 1;
    int got = 0;
    int started = 1;
    int next = 101;
    int guard = 0;
    if (mss == NULL) {
        return -1;
    }
    while (next < 101 + 18000 && guard < 40) {
        int ack = next + 1000;
        next = ack;
        got++;
        guard++;
        if (started < thresh && cwnd < thresh) {
            cwnd++;
        }
        if (got == budget) {
            if (started >= thresh) {
                cwnd++;
            }
            got = 0;
            started = cwnd;
            budget = cwnd;
        }
        if (ack == ackno) {
            *mss = cwnd;
            return 0;
        }
    }
    return -1;
}

static int line_ok(int n, int cap) { return n > 0 && n < cap ? 0 : -1; }

static int line_2016(char *out, int cap) {
    int rwnd = 0;
    int cwnd = 0;
    int swnd = 0;
    unsigned seq = 0;
    int rounds = 0;
    int rate = 0;
    int n;
    if (net_cong_2016(&rwnd, &cwnd, &swnd, &seq, &rounds, &rate) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "rwnd=%d cwnd=%d swnd=%d seq=%u rtt=%d kbps=%d.%02d", rwnd, cwnd, swnd, seq,
                 rounds, rate / 100, rate % 100);
    return line_ok(n, cap);
}

int net_cong_line(const char *id, char *out, int cap) {
    int cwnd = 0;
    int swnd = 0;
    int rwnd = 0;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2009-39") == 0) {
        if (net_cong_run(16, 0, -1, 0, 1, 4, &cwnd, &swnd, &rwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "kb=%d letter=C", cwnd);
    } else if (strcmp(id, "2014-38") == 0) {
        if (net_cong_run(8, 0, 10, 0, 1, 10, &cwnd, &swnd, &rwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "cwnd=%d swnd=%d letter=A", cwnd, swnd);
    } else if (strcmp(id, "2015-39") == 0) {
        if (net_cong_run(1, 32, -1, 16, 0, 4, &cwnd, &swnd, &rwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "cwnd=%d swnd=%d letter=A", cwnd, swnd);
    } else if (strcmp(id, "2016-41") == 0) {
        return line_2016(out, cap);
    } else if (strcmp(id, "2017-39") == 0) {
        int rtts = net_cong_span(1, 32, 1);
        n = snprintf(out, (size_t)cap, "rtt=%d ms=%d letter=A", rtts, rtts * 5);
    } else if (strcmp(id, "2020-38") == 0) {
        int rtts = net_cong_span(8, 32, 0);
        n = snprintf(out, (size_t)cap, "rtt=%d ms=%d letter=D", rtts, rtts * 2);
    } else if (strcmp(id, "2022-38") == 0) {
        n = snprintf(out, (size_t)cap, "rtt=%d letter=C", net_cong_back(16));
    } else if (strcmp(id, "fast") == 0) {
        int thresh = 0;
        if (net_cong_fast(16, &thresh, &cwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "cwnd=%d thresh=%d", cwnd, thresh);
    } else if (strcmp(id, "cap") == 0) {
        if (net_cong_run(8, 12, -1, 0, 0, 1, &cwnd, &swnd, &rwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "cwnd=%d", cwnd);
    } else {
        return -1;
    }
    return line_ok(n, cap);
}
