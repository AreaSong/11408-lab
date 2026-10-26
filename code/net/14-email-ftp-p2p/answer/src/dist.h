#ifndef DIST_H
#define DIST_H

#include "ratio.h"

/*
 * 客户-服务器：Dcs = max(NF/us, F/dmin)
 * 对等方：Dp2p = max(F/us, F/dmin, NF/(us+Σui))
 * 假定核心网络不拥塞，瓶颈只在接入链路。文件一开始只在服务器上。
 */
typedef struct {
    long long file_bits;
    long long server_bps;
    long long dmin;
    long long sum_up;
    long long one_down;
    long long one_up;
    int n;
    int uniform;
    int counted;
} Dist;

typedef struct {
    Ratio nf_us;
    Ratio f_dmin;
    Ratio f_us;
    Ratio nf_total;
    Ratio cs;
    Ratio p2p;
    long long total_up;
    int cs_mask;
    int p2p_mask;
} DistOut;

const char *dist_add(Dist *d, long long down, long long up);
const char *dist_count(Dist *d, int n);
const char *dist_scale(const Dist *src, int n, Dist *dst);
const char *dist_eval(const Dist *d, DistOut *out);

#endif
