#ifndef K_BANK_H
#define K_BANK_H

#include <stdio.h>

/* 五个进程、三类资源，盖住 2011、2012、2018、2022 和教科书那张五进程表。 */
#define K_BANK_N 5
#define K_BANK_M 3

typedef struct {
    int n;
    int m;
    int base;
    int avail[K_BANK_M];
    int maxv[K_BANK_N][K_BANK_M];
    int alloc[K_BANK_N][K_BANK_M];
    int need[K_BANK_N][K_BANK_M];
} KBank;

typedef struct {
    int safe;
    int nstep;
    int order[K_BANK_N];
    int work[K_BANK_N][K_BANK_M];
    int need[K_BANK_N][K_BANK_M];
    int alloc[K_BANK_N][K_BANK_M];
    int next[K_BANK_N][K_BANK_M];
    int stuck[K_BANK_M];
} KSafe;

enum { K_GRANT = 0, K_EXCEED = 1, K_WAIT = 2, K_UNSAFE = 3 };

/* rows_are_need 为 1 时，调用前填的是 Need，最大需求由已分配加出来。 */
int k_bank_finish(KBank *b, int rows_are_need);
int k_bank_from_total(KBank *b, const int *total);
int k_bank_safe(const KBank *b, KSafe *out);
/* 这条顺序走得通返回 1。卡住返回 0，fail 是那个进程的下标。顺序不合法返回 -1。 */
int k_bank_check(const KBank *b, const int *order, int n, int *fail);
int k_bank_count(const KBank *b);
/* seqs 可以是 NULL。条数写进 total，最多留下 cap 条，按编号从小到大的字典序。 */
int k_bank_seqs(const KBank *b, int seqs[][K_BANK_N], int cap, int *total);
int k_bank_request(const KBank *b, int who, const int *req, int *kind, KSafe *out);

void k_bank_print(const KBank *b, const KSafe *safe, int count, FILE *fp);
void k_bank_print_check(const KBank *b, const int *order, int n, int ok, int fail, FILE *fp);
void k_bank_print_req(const KBank *b, int who, const int *req, int kind, const KSafe *safe, FILE *fp);

#endif
