#include "pipe.h"

#include "qty.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool err_msg(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return false;
}

static int mul_ok(long long a, long long b, long long *out) {
    if (a < 0 || b < 0) {
        return 0;
    }
    if (a > 0 && b > LLONG_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static int add_ok(long long a, long long b, long long *out) {
    if (a < 0 || b < 0 || a > LLONG_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

/* 半入进一。用减法比较，避免余数乘 2 溢出。 */
static int div_round(long long num, long long den, long long *out) {
    long long q;
    long long rem;

    if (den <= 0 || num < 0) {
        return 0;
    }
    q = num / den;
    rem = num % den;
    if (rem >= den - rem) {
        if (q == LLONG_MAX) {
            return 0;
        }
        q++;
    }
    *out = q;
    return 1;
}

bool pipe_delays(long long frame_bits, long long ack_bits, int ack_same, long long bps,
                 long long prop_ns, PipeDelay *d, char *err, int cap) {
    long long td = 0;
    long long ta = 0;
    long long cycle = 0;

    if (d == NULL || frame_bits <= 0 || bps <= 0 || prop_ns < 0 || (!ack_same && ack_bits < 0)) {
        return err_msg(err, cap, "帧长和带宽要是正数，传播时延、确认帧不能是负的");
    }
    if (ack_same) {
        ack_bits = frame_bits;
    }
    if (!qty_div_ns(frame_bits, bps, &td) || !qty_div_ns(ack_bits, bps, &ta)) {
        return err_msg(err, cap, "发送时延除不尽。本关只收能落在整数纳秒上的数据");
    }
    if (prop_ns > LLONG_MAX / 2 || !add_ok(td, ta, &cycle) || !add_ok(cycle, prop_ns * 2, &cycle)) {
        return err_msg(err, cap, "确认周期溢出");
    }
    d->frame_bits = frame_bits;
    d->ack_bits = ack_bits;
    d->bps = bps;
    d->prop_ns = prop_ns;
    d->td_ns = td;
    d->ta_ns = ta;
    d->cycle_ns = cycle;
    return true;
}

int pipe_window_for(const PipeDelay *d, int percent) {
    long long num;
    long long den;
    long long w;

    if (d == NULL || d->td_ns <= 0 || d->cycle_ns <= 0 || percent < 1 || percent > 100) {
        return 0;
    }
    if (!mul_ok(d->cycle_ns, percent, &num) || !mul_ok(d->td_ns, 100, &den)) {
        return 0;
    }
    w = (num + den - 1) / den;
    if (w < 1 || w > 1000000) {
        return 0;
    }
    return (int)w;
}

static int util_of(const PipeDelay *d, int window, int saturated, int *milli) {
    long long num;
    long long q;

    if (saturated) {
        *milli = 100000;
        return 1;
    }
    if (!mul_ok((long long)window, d->td_ns, &num) || !mul_ok(num, 100000, &num)) {
        return 0;
    }
    if (!div_round(num, d->cycle_ns, &q) || q < 0 || q > 100000) {
        return 0;
    }
    *milli = (int)q;
    return 1;
}

static int exact_bps(const PipeDelay *d, int window, PipeRate *r) {
    long long bits;
    long long num;

    r->rate_exact = 0;
    r->rate_bps = 0;
    if (!mul_ok(d->frame_bits, window, &bits) || !mul_ok(bits, 1000000000LL, &num)) {
        return 0;
    }
    if (num % d->cycle_ns == 0) {
        r->rate_exact = 1;
        r->rate_bps = num / d->cycle_ns;
    }
    return 1;
}

static int approx_mbps(const PipeDelay *d, int window, PipeRate *r) {
    long long bits;
    long long num;
    long long milli;

    r->rate_milli = 0;
    r->rate_whole = 0;
    r->rate_approx = 0;
    if (!mul_ok(d->frame_bits, window, &bits) || !mul_ok(bits, 1000000LL, &num)) {
        return 0;
    }
    if (!div_round(num, d->cycle_ns, &milli) || milli > INT_MAX) {
        return 0;
    }
    r->rate_milli = (int)milli;
    r->rate_whole = (int)((milli + 500) / 1000);
    r->rate_approx = milli % 1000 != 0;
    return 1;
}

bool pipe_rate(const PipeDelay *d, int window, PipeRate *r, char *err, int cap) {
    int full;

    if (d == NULL || r == NULL || window < 1) {
        return err_msg(err, cap, "窗口至少是 1");
    }
    full = pipe_window_for(d, 100);
    if (full < 1) {
        return err_msg(err, cap, "满载窗口算不出来");
    }
    memset(r, 0, sizeof *r);
    r->window = window;
    r->full_window = full;
    r->saturated = window >= full;
    if (!util_of(d, window, r->saturated, &r->util_milli)) {
        return err_msg(err, cap, "利用率溢出");
    }
    if (r->saturated) {
        r->rate_exact = 1;
        r->rate_bps = d->bps;
        return true;
    }
    if (!exact_bps(d, window, r) || !approx_mbps(d, window, r)) {
        return err_msg(err, cap, "速率溢出");
    }
    return true;
}
