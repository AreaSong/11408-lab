#include "dist.h"

#include <limits.h>
#include <string.h>

const char *dist_add(Dist *d, long long down, long long up) {
    __int128 sum;

    if (d == NULL || down <= 0 || up < 0) {
        return "下载速率要大于 0";
    }
    if (d->counted) {
        return "count 之后不能再加 peer，先 reset";
    }
    if (d->n >= 1000000) {
        return "人数太多";
    }
    sum = (__int128)d->sum_up + up;
    if (sum > LLONG_MAX) {
        return "上传速率之和太大";
    }
    if (d->n == 0) {
        d->one_down = down;
        d->one_up = up;
        d->dmin = down;
        d->uniform = 1;
    } else {
        if (down < d->dmin) {
            d->dmin = down;
        }
        if (down != d->one_down || up != d->one_up) {
            d->uniform = 0;
        }
    }
    d->sum_up = (long long)sum;
    d->n++;
    return NULL;
}

const char *dist_count(Dist *d, int n) {
    __int128 sum;

    if (d == NULL || d->n < 1) {
        return "要先写 peer";
    }
    if (!d->uniform) {
        return "对等方速率不一致，不能用 count 改人数";
    }
    if (n < 1 || n > 1000000) {
        return "人数要在 1 到 1000000 之间";
    }
    sum = (__int128)d->one_up * n;
    if (sum > LLONG_MAX) {
        return "上传速率之和太大";
    }
    d->n = n;
    d->sum_up = (long long)sum;
    d->dmin = d->one_down;
    d->counted = 1;
    return NULL;
}

const char *dist_scale(const Dist *src, int n, Dist *dst) {
    if (src == NULL || dst == NULL || src->n < 1) {
        return "要先写 peer";
    }
    if (!src->uniform) {
        return "curve 要求每个对等方的上下载速率相同";
    }
    if (n < 1 || n > 1000000) {
        return "人数要在 1 到 1000000 之间";
    }
    memset(dst, 0, sizeof *dst);
    dst->file_bits = src->file_bits;
    dst->server_bps = src->server_bps;
    dst->one_down = src->one_down;
    dst->one_up = src->one_up;
    dst->dmin = src->one_down;
    dst->sum_up = src->one_up;
    dst->n = 1;
    dst->uniform = 1;
    return dist_count(dst, n);
}

static int term(long long num, long long den, Ratio *out) {
    __int128 prod;

    if (den <= 0 || num < 0) {
        return -1;
    }
    prod = num;
    if (prod > LLONG_MAX) {
        return -1;
    }
    return ratio_make((long long)prod, den, out);
}

static int mask_of(Ratio a, Ratio b, Ratio c, Ratio top) {
    int mask = 0;

    if (ratio_cmp(a, top) == 0) {
        mask |= 1;
    }
    if (ratio_cmp(b, top) == 0) {
        mask |= 2;
    }
    if (ratio_cmp(c, top) == 0) {
        mask |= 4;
    }
    return mask;
}

const char *dist_eval(const Dist *d, DistOut *out) {
    __int128 total;
    __int128 nf;
    Ratio top;

    if (d == NULL || out == NULL) {
        return "缺少参数";
    }
    if (d->file_bits <= 0 || d->server_bps <= 0) {
        return "要先写 file 和 server";
    }
    if (d->n < 1 || d->dmin <= 0) {
        return "至少要有一个对等方";
    }
    total = (__int128)d->server_bps + d->sum_up;
    nf = (__int128)d->n * d->file_bits;
    if (total > LLONG_MAX || nf > LLONG_MAX) {
        return "文件或速率太大，乘积溢出";
    }
    memset(out, 0, sizeof *out);
    out->total_up = (long long)total;
    if (term((long long)nf, d->server_bps, &out->nf_us) != 0 ||
        term(d->file_bits, d->dmin, &out->f_dmin) != 0 ||
        term(d->file_bits, d->server_bps, &out->f_us) != 0 ||
        term((long long)nf, out->total_up, &out->nf_total) != 0) {
        return "时间算不出来";
    }
    top = ratio_cmp(out->nf_us, out->f_dmin) >= 0 ? out->nf_us : out->f_dmin;
    out->cs = top;
    out->cs_mask = 0;
    if (ratio_cmp(out->nf_us, top) == 0) {
        out->cs_mask |= 1;
    }
    if (ratio_cmp(out->f_dmin, top) == 0) {
        out->cs_mask |= 2;
    }
    top = out->f_us;
    if (ratio_cmp(out->f_dmin, top) > 0) {
        top = out->f_dmin;
    }
    if (ratio_cmp(out->nf_total, top) > 0) {
        top = out->nf_total;
    }
    out->p2p = top;
    out->p2p_mask = mask_of(out->f_us, out->f_dmin, out->nf_total, top);
    return NULL;
}
