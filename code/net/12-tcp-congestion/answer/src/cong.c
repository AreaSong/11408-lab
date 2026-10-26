#include "cong.h"

#include <string.h>

static const char *need_mss(const Cong *c) {
    if (c->mss <= 0) {
        return "先设置 mss";
    }
    return NULL;
}

static const char *multiple(const Cong *c, long n, const char *what) {
    const char *err = need_mss(c);

    if (err != NULL) {
        return err;
    }
    if (n <= 0 || n % c->mss != 0) {
        return what;
    }
    return NULL;
}

void cong_clear(Cong *c) {
    memset(c, 0, sizeof *c);
}

const char *cong_set_mss(Cong *c, long mss) {
    if (c->mss != 0) {
        return "mss 已经设过";
    }
    if (mss <= 0) {
        return "mss 要是正数";
    }
    c->mss = mss;
    return NULL;
}

const char *cong_set_rtt(Cong *c, long us) {
    if (us <= 0) {
        return "RTT 要是正数";
    }
    c->rtt_us = us;
    return NULL;
}

const char *cong_set_buf(Cong *c, long n) {
    const char *err = multiple(c, n, "接收缓存要是 MSS 的整数倍");

    if (err != NULL) {
        return err;
    }
    if (c->rwnd_fixed) {
        return "固定接收窗口和接收缓存只能设一个";
    }
    c->buf = n;
    return NULL;
}

const char *cong_set_rwnd(Cong *c, long n) {
    const char *err = multiple(c, n, "接收窗口要是 MSS 的整数倍");

    if (err != NULL) {
        return err;
    }
    if (c->buf > 0) {
        return "固定接收窗口和接收缓存只能设一个";
    }
    c->rwnd = n;
    c->rwnd_fixed = 1;
    return NULL;
}

const char *cong_set_seq(Cong *c, long seq) {
    if (seq < 0) {
        return "序号不能是负数";
    }
    c->next_seq = seq;
    c->seq_set = 1;
    return NULL;
}

const char *cong_init(Cong *c, long cwnd, long ssthresh) {
    const char *err = multiple(c, cwnd, "拥塞窗口要是 MSS 的整数倍");

    if (err != NULL) {
        return err;
    }
    err = multiple(c, ssthresh, "慢开始门限要是 MSS 的整数倍");
    if (err != NULL) {
        return err;
    }
    if (c->ready) {
        return "初始窗口已经设过";
    }
    c->cwnd = cwnd;
    c->ssthresh = ssthresh;
    c->ready = 1;
    c->ca_base = cwnd;
    return NULL;
}

long cong_rwnd(const Cong *c) {
    long free_bytes;

    if (c->buf > 0) {
        free_bytes = c->buf - c->buffered;
        return free_bytes > 0 ? free_bytes : 0;
    }
    if (c->rwnd_fixed) {
        return c->rwnd;
    }
    return -1;
}

long cong_swnd(const Cong *c) {
    long rwnd = cong_rwnd(c);

    if (rwnd < 0 || c->cwnd < rwnd) {
        return c->cwnd;
    }
    return rwnd;
}

static const char *need_ready(const Cong *c) {
    if (!c->ready) {
        return "先 init";
    }
    return NULL;
}

/* 谢希仁写「不能小于 2」。统考这几题减半之后都比 2 个 MSS 大。 */
static long half_thresh(const Cong *c, long cwnd) {
    long half = (cwnd / c->mss) / 2;

    if (half < 2) {
        half = 2;
    }
    return half * c->mss;
}

const char *cong_timeout(Cong *c, long *old_cwnd) {
    const char *err = need_ready(c);

    if (err != NULL) {
        return err;
    }
    *old_cwnd = c->cwnd;
    c->ssthresh = half_thresh(c, c->cwnd);
    c->cwnd = c->mss;
    c->in_flight = 0;
    c->dup_acks = 0;
    c->rounds = 0;
    c->ca_credit = 0;
    c->ca_base = c->cwnd;
    return NULL;
}

static long sendable(const Cong *c) {
    long room = cong_swnd(c) - c->in_flight;

    if (room < c->mss) {
        return 0;
    }
    return (room / c->mss) * c->mss;
}

static void fill_end(Cong *c, RoundSnap *snap) {
    snap->cwnd1 = c->cwnd;
    snap->rwnd1 = cong_rwnd(c);
    snap->swnd1 = cong_swnd(c);
    snap->limited = cong_rwnd(c) >= 0;
    snap->use_buf = c->buf > 0;
    snap->buffered1 = c->buffered;
    snap->index = c->rounds;
}

static void grow_slow(Cong *c, long sent, RoundSnap *snap) {
    long sum = c->cwnd + sent;

    snap->capped = sum > c->ssthresh;
    c->cwnd = snap->capped ? c->ssthresh : sum;
    c->ca_credit = 0;
    c->ca_base = c->cwnd;
}

const char *cong_round(Cong *c, RoundSnap *snap) {
    const char *err = need_ready(c);
    long sent;

    if (err != NULL) {
        return err;
    }
    if (c->in_flight != 0) {
        return "还有在途数据，这一轮还没结束";
    }
    sent = sendable(c);
    if (sent <= 0) {
        return "发送窗口小于 1 个 MSS，这一轮没有新数据";
    }
    memset(snap, 0, sizeof *snap);
    snap->slow = c->cwnd < c->ssthresh;
    snap->sent = sent;
    snap->cwnd0 = c->cwnd;
    snap->ssthresh = c->ssthresh;
    if (c->buf > 0) {
        c->buffered += sent;
    }
    if (c->seq_set) {
        c->next_seq += sent;
    }
    c->new_acks += (int)(sent / c->mss);
    c->dup_acks = 0;
    if (snap->slow) {
        grow_slow(c, sent, snap);
    } else {
        /* 做题口径：拥塞避免按轮加 1 个 MSS，不要求这一轮把整个 cwnd 都发完。 */
        c->cwnd += c->mss;
        c->ca_credit = 0;
        c->ca_base = c->cwnd;
    }
    c->rounds += 1;
    fill_end(c, snap);
    return NULL;
}

const char *cong_send(Cong *c, long *sent) {
    const char *err = need_ready(c);
    long n;

    if (err != NULL) {
        return err;
    }
    n = sendable(c);
    if (n <= 0) {
        return "现在发不出一个 MSS";
    }
    c->in_flight += n;
    if (c->seq_set) {
        c->next_seq += n;
    }
    *sent = n;
    return NULL;
}

static void grow_one_ack(Cong *c) {
    if (c->cwnd < c->ssthresh) {
        long next = c->cwnd + c->mss;

        c->cwnd = next > c->ssthresh ? c->ssthresh : next;
        c->ca_credit = 0;
        c->ca_base = c->cwnd;
        return;
    }
    if (c->ca_base <= 0) {
        c->ca_base = c->cwnd;
    }
    c->ca_credit += c->mss;
    if (c->ca_credit >= c->ca_base) {
        c->cwnd += c->mss;
        c->ca_credit -= c->ca_base;
        c->ca_base = c->cwnd;
    }
}

const char *cong_ack(Cong *c, int n) {
    const char *err = need_ready(c);
    long bytes;
    int i;

    if (err != NULL) {
        return err;
    }
    if (n <= 0) {
        return "确认的段数要是正数";
    }
    bytes = (long)n * c->mss;
    if (bytes > c->in_flight) {
        return "确认超过了在途数据";
    }
    c->in_flight -= bytes;
    if (c->buf > 0) {
        c->buffered += bytes;
    }
    c->dup_acks = 0;
    for (i = 0; i < n; i++) {
        c->new_acks += 1;
        grow_one_ack(c);
    }
    return NULL;
}

const char *cong_dup(Cong *c, int *fired, long *old_cwnd) {
    const char *err = need_ready(c);

    if (err != NULL) {
        return err;
    }
    c->dup_acks += 1;
    *fired = 0;
    *old_cwnd = c->cwnd;
    if (c->dup_acks == 3) {
        *fired = 1;
        c->ssthresh = half_thresh(c, c->cwnd);
        c->cwnd = c->ssthresh;
        c->ca_credit = 0;
        c->ca_base = c->cwnd;
    }
    return NULL;
}

const char *cong_until(Cong *c, int kind, long target, RoundSnap *snaps, int cap,
                       int *n_out) {
    const char *err = need_ready(c);
    int n = 0;

    if (err != NULL) {
        return err;
    }
    while (n < cap) {
        long cur = kind == UNTIL_SWND ? cong_swnd(c) : c->cwnd;

        if (cur >= target) {
            *n_out = n;
            return NULL;
        }
        err = cong_round(c, &snaps[n]);
        if (err != NULL) {
            return err;
        }
        n++;
    }
    return "轮数超过上限";
}

const char *cong_stall(Cong *c, RoundSnap *snaps, int cap, StallInfo *info) {
    const char *err = need_ready(c);
    long seq0 = c->next_seq;
    int n = 0;

    if (err != NULL) {
        return err;
    }
    if (c->buf <= 0) {
        return "要先给接收缓存，发送窗口才会收到 0";
    }
    if (c->rtt_us <= 0) {
        return "先设置 rtt，才能算平均速率";
    }
    if (!c->seq_set) {
        return "先设置序号";
    }
    while (cong_swnd(c) >= c->mss) {
        if (n >= cap) {
            return "轮数超过上限";
        }
        err = cong_round(c, &snaps[n]);
        if (err != NULL) {
            return err;
        }
        n++;
    }
    info->rounds = n;
    info->bytes = c->next_seq - seq0;
    info->next_seq = c->next_seq;
    info->seq_set = 1;
    info->rtt_us = c->rtt_us;
    info->snap_n = n;
    return NULL;
}
