#ifndef K_DEAD_H
#define K_DEAD_H

#include "kernel/k_bank.h"

/*
 * 资源分配图。申请是「此刻还要的」，不是银行家的 Need。
 * 化简回答现在是不是已经死锁；银行家回答将来会不会走进死锁。
 */
typedef struct {
    int n;
    int m;
    int base;
    int avail[K_BANK_M];
    int alloc[K_BANK_N][K_BANK_M];
    int req[K_BANK_N][K_BANK_M];
} KRag;

typedef struct {
    int deadlock;
    int cycle;
    int nstep;
    int step_who[K_BANK_N];
    int step_work[K_BANK_N][K_BANK_M];
    int step_req[K_BANK_N][K_BANK_M];
    int step_alloc[K_BANK_N][K_BANK_M];
    int step_next[K_BANK_N][K_BANK_M];
    int nremoved;
    int removed[K_BANK_N];
    int nleft;
    int left[K_BANK_N];
    int niso;
    int isolated[K_BANK_N];
    int ncycle;
    int cyc[K_BANK_N];
    int stuck[K_BANK_M];
} KRed;

typedef struct {
    int recovered;
    int naborts;
    int who[K_BANK_N];
    int cost[K_BANK_N];
    int avail[K_BANK_N][K_BANK_M];
} KRecover;

int k_rag_reduce(const KRag *g, KRed *out);
int k_rag_recover(const KRag *g, const int *cost, KRecover *out);

/* 每人最多 k[i] 个时，保证不死锁的最少资源是 Σ(k-1)+1。 */
int k_bound_list(const int *k, int n, long long *least);
int k_bound_equal(int n, int k, long long *least);
/* m 个资源、每人最多 k 个时，可能死锁的最少进程数。2009 年第 25 题就是这个数。 */
int k_bound_mink(int resources, int each, int *out);

/* 2015-26、2019-30 的句子。不认识返回 NULL，认识返回 "true" 或 "false"。 */
const char *k_claim(const char *name);

void k_dead_print(const KRag *g, const KRed *out, FILE *fp);
void k_recover_print(const KRag *g, const KRecover *out, FILE *fp);

#endif
