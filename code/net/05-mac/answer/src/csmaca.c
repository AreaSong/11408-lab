#include "csmaca.h"

#include <limits.h>
#include <stddef.h>

static int add_ns(long long a, long long b, long long *out) {
    if (a < 0 || b < 0 || a > LLONG_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

int ca_nav(long long sifs_ns, long long data_ns, long long ack_ns, long long *nav_ns) {
    long long two;
    long long sum;

    if (nav_ns == NULL || sifs_ns <= 0 || data_ns <= 0 || ack_ns <= 0) {
        return 0;
    }
    if (sifs_ns > LLONG_MAX / 2) {
        return 0;
    }
    two = sifs_ns * 2;
    if (!add_ns(two, data_ns, &sum) || !add_ns(sum, ack_ns, nav_ns)) {
        return 0;
    }
    return 1;
}

int ca_time(const CaSpec *in, CaTime *out) {
    long long back;
    long long nav = 0;
    long long total = 0;
    long long part[9];
    int i;

    if (in == NULL || out == NULL) {
        return 0;
    }
    if (in->sifs_ns <= 0 || in->difs_ns <= in->sifs_ns || in->slot_ns <= 0) {
        return 0;
    }
    if (in->backoff < 0 || in->backoff > 1023) {
        return 0;
    }
    if (in->rts_ns <= 0 || in->cts_ns <= 0 || in->data_ns <= 0 || in->ack_ns <= 0) {
        return 0;
    }
    if (in->backoff > 0 && in->slot_ns > LLONG_MAX / in->backoff) {
        return 0;
    }
    back = (long long)in->backoff * in->slot_ns;
    if (!ca_nav(in->sifs_ns, in->data_ns, in->ack_ns, &nav)) {
        return 0;
    }
    part[0] = in->difs_ns;
    part[1] = back;
    part[2] = in->rts_ns;
    part[3] = in->sifs_ns;
    part[4] = in->cts_ns;
    part[5] = in->sifs_ns;
    part[6] = in->data_ns;
    part[7] = in->sifs_ns;
    part[8] = in->ack_ns;
    for (i = 0; i < 9; i++) {
        if (!add_ns(total, part[i], &total)) {
            return 0;
        }
    }
    out->backoff_ns = back;
    out->nav_cts_ns = nav;
    out->total_ns = total;
    return 1;
}
