#include "rtt.h"

#include <string.h>

void rtt_clear(RttEst *e) {
    memset(e, 0, sizeof *e);
}

const char *rtt_set_rto(RttEst *e, long us) {
    if (us <= 0) {
        return "超时重传时间要是正数";
    }
    if (e->have) {
        return "已经有 RTT 样本，初始超时不再改";
    }
    e->rto_us = us;
    return NULL;
}

/* (1 - n/d) * old + (n/d) * sample，n/d 是 1/8 或 1/4，先乘后除保持整微秒。 */
static long smooth(long old, long sample, int numer, int denom) {
    return (long)(((long long)(denom - numer) * old + (long long)numer * sample) / denom);
}

static long abs_diff(long a, long b) {
    return a >= b ? a - b : b - a;
}

static void first_sample(RttEst *e, long sample, RttStep *step) {
    e->rtts_us = sample;
    e->rttd_us = sample / 2;
    e->rto_us = e->rtts_us + 4 * e->rttd_us;
    e->have = 1;
    step->first = 1;
    step->rtts_us = e->rtts_us;
    step->rttd_us = e->rttd_us;
    step->rto_us = e->rto_us;
}

static void next_sample(RttEst *e, long sample, RttStep *step) {
    long old_rtts = e->rtts_us;
    long old_rttd = e->rttd_us;
    long new_rtts = smooth(old_rtts, sample, 1, 8);
    long wangdao_gap = abs_diff(new_rtts, sample);
    long rfc_gap = abs_diff(old_rtts, sample);

    step->old_rtts_us = old_rtts;
    step->old_rttd_us = old_rttd;
    e->rtts_us = new_rtts;
    /* 王道 / 谢希仁习题：先更新 RTTs，偏差用新值和样本的差。 */
    e->rttd_us = smooth(old_rttd, wangdao_gap, 1, 4);
    e->rto_us = e->rtts_us + 4 * e->rttd_us;
    step->rtts_us = e->rtts_us;
    step->rttd_us = e->rttd_us;
    step->rto_us = e->rto_us;
    step->rfc_rto_us = new_rtts + 4 * smooth(old_rttd, rfc_gap, 1, 4);
}

const char *rtt_sample(RttEst *e, long sample_us, RttStep *step) {
    if (sample_us <= 0) {
        return "RTT 样本要是正数";
    }
    memset(step, 0, sizeof *step);
    step->sample_us = sample_us;
    step->prev_rto_us = e->rto_us;
    if (e->ignore) {
        e->ignore = 0;
        step->ignored = 1;
        step->rtts_us = e->rtts_us;
        step->rttd_us = e->rttd_us;
        step->rto_us = e->rto_us;
        return NULL;
    }
    if (!e->have) {
        first_sample(e, sample_us, step);
        return NULL;
    }
    next_sample(e, sample_us, step);
    return NULL;
}

const char *rtt_rexmit(RttEst *e, long *rto_us) {
    if (e->rto_us <= 0) {
        return "还没有超时重传时间";
    }
    if (e->rto_us > 86400L * 1000000L / 2) {
        return "超时重传时间翻倍后太大";
    }
    e->rto_us *= 2;
    e->ignore = 1;
    *rto_us = e->rto_us;
    return NULL;
}
