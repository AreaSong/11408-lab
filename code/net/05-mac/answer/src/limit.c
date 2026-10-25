#include "limit.h"

#include "qty.h"

#include <limits.h>
#include <string.h>

static int same(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* 与第 00 关相同：只接受离整数极近的换算，不在这里四舍五入。 */
static int scale_ll(double value, long long scale, long long *out) {
    long double prod;
    long long rounded;
    long double err;

    if (!(value >= 0) || value > 1e15 || scale <= 0) {
        return 0;
    }
    prod = (long double)value * (long double)scale;
    if (prod > (long double)LLONG_MAX) {
        return 0;
    }
    rounded = (long long)(prod + 0.5L);
    err = prod - (long double)rounded;
    if (err < 0) {
        err = -err;
    }
    if (err > 0.001L) {
        return 0;
    }
    *out = rounded;
    return 1;
}

bool parse_speed(double value, const char *unit, long long *mps) {
    long long scale = 0;
    long long raw = 0;

    if (unit == NULL || mps == NULL) {
        return false;
    }
    if (same(unit, "m/s") || same(unit, "mps")) {
        scale = 1;
    } else if (same(unit, "km/s")) {
        scale = 1000;
    } else if (same(unit, "m/us")) {
        scale = 1000000LL;
    } else {
        return false;
    }
    if (!scale_ll(value, scale, &raw) || raw <= 0) {
        return false;
    }
    *mps = raw;
    return true;
}

bool span_times(long long bits, long long bps, long long *send_ns, long long *half_ns) {
    if (bits < 0 || bps <= 0 || bps > LLONG_MAX / 2) {
        return false;
    }
    if (!qty_div_ns(bits, bps, send_ns)) {
        return false;
    }
    if (!qty_mul_div(bits, 1000000000LL, 2 * bps, half_ns)) {
        return false;
    }
    return *send_ns == 2 * *half_ns;
}

bool span_by_frame(long long bits, long long bps, long long mps, long long dev_ns, Span *o) {
    if (o == NULL || mps <= 0 || dev_ns < 0) {
        return false;
    }
    memset(o, 0, sizeof *o);
    o->bits = bits;
    o->dev_ns = dev_ns;
    if (!span_times(bits, bps, &o->send_ns, &o->half_ns)) {
        return false;
    }
    if (o->half_ns < dev_ns) {
        return false;
    }
    o->prop_ns = o->half_ns - dev_ns;
    return qty_mul_div(o->prop_ns, mps, 1000000000LL, &o->meters);
}

bool span_by_distance(long long bps, long long meters, long long mps, long long dev_ns, Span *o) {
    long long prop_bits = 0;
    long long dev_bits = 0;

    if (o == NULL || bps <= 0 || bps > LLONG_MAX / 2 || mps <= 0 || meters < 0 || dev_ns < 0) {
        return false;
    }
    if (!qty_mul_div(2 * bps, meters, mps, &prop_bits)) {
        return false;
    }
    if (dev_ns > 0 && !qty_mul_div(2 * bps, dev_ns, 1000000000LL, &dev_bits)) {
        return false;
    }
    if (prop_bits > LLONG_MAX - dev_bits) {
        return false;
    }
    return span_by_frame(prop_bits + dev_bits, bps, mps, dev_ns, o);
}

bool span_delta(long long bits, long long bps, long long mps, long long *rtt_ns, long long *one_ns,
                long long *meters) {
    if (mps <= 0 || bps <= 0 || bps > LLONG_MAX / 2) {
        return false;
    }
    if (!qty_div_ns(bits, bps, rtt_ns)) {
        return false;
    }
    if (!qty_mul_div(bits, 1000000000LL, 2 * bps, one_ns)) {
        return false;
    }
    if (*rtt_ns != 2 * *one_ns) {
        return false;
    }
    return qty_mul_div(*one_ns, mps, 1000000000LL, meters);
}

bool span_prop(long long meters, long long mps, long long *tau_ns) {
    if (meters < 0 || mps <= 0) {
        return false;
    }
    return qty_mul_div(meters, 1000000000LL, mps, tau_ns);
}

bool backoff_calc(int collisions, long long slot_ns, Backoff *o) {
    int k;
    int rmax;

    if (o == NULL || collisions < 1 || collisions > 16 || slot_ns <= 0) {
        return false;
    }
    memset(o, 0, sizeof *o);
    o->collisions = collisions;
    o->slot_ns = slot_ns;
    if (collisions >= 16) {
        o->k = 0;
        o->rmax = -1;
        o->max_ns = 0;
        return true;
    }
    k = collisions < 10 ? collisions : 10;
    rmax = (1 << k) - 1;
    if (rmax > 0 && slot_ns > LLONG_MAX / rmax) {
        return false;
    }
    o->k = k;
    o->rmax = rmax;
    o->max_ns = (long long)rmax * slot_ns;
    return true;
}

bool backoff_chosen(int collisions, int r, long long slot_ns, Backoff *o, long long *wait_ns) {
    if (!backoff_calc(collisions, slot_ns, o) || o->rmax < 0 || r < 0 || r > o->rmax) {
        return false;
    }
    if (r > 0 && slot_ns > LLONG_MAX / r) {
        return false;
    }
    *wait_ns = (long long)r * slot_ns;
    return true;
}

bool goodput_calc(Goodput *g) {
    long long rtt;
    long long num;

    if (g == NULL || g->bps <= 0 || g->tau_ns < 0 || g->data_bits < 0) {
        return false;
    }
    if (g->tau_ns > LLONG_MAX / 2) {
        return false;
    }
    if (!qty_div_ns(g->frame_bits, g->bps, &g->frame_ns)) {
        return false;
    }
    if (!qty_div_ns(g->ack_bits, g->bps, &g->ack_ns)) {
        return false;
    }
    rtt = g->tau_ns * 2;
    if (g->frame_ns > LLONG_MAX - g->ack_ns || g->frame_ns + g->ack_ns > LLONG_MAX - rtt) {
        return false;
    }
    g->period_ns = g->frame_ns + g->ack_ns + rtt;
    if (g->period_ns <= 0 || g->data_bits > LLONG_MAX / 1000000000LL) {
        return false;
    }
    num = g->data_bits * 1000000000LL;
    g->out_bps = num / g->period_ns;
    g->exact = num % g->period_ns == 0;
    if (g->out_bps > LLONG_MAX - 5000) {
        return false;
    }
    g->centi = (int)((g->out_bps + 5000) / 10000);
    return true;
}
