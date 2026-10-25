#ifndef BANK_H
#define BANK_H

#include "num.h"

typedef struct {
    int n;
    int m;
    int base;
    int avail[BANK_M];
    int maxv[BANK_N][BANK_M];
    int alloc[BANK_N][BANK_M];
    int need[BANK_N][BANK_M];
} Bank;

typedef struct {
    int nstep;
    TraceStep steps[BANK_N];
    int safe;
    int order[BANK_N];
    int stuck[BANK_M];
    int open[BANK_N];
    int nopen;
} SafeTrace;

typedef enum {
    REQ_GRANT = 0,
    REQ_EXCEED = 1,
    REQ_WAIT = 2,
    REQ_UNSAFE = 3
} ReqKind;

typedef struct {
    ReqKind kind;
    SafeTrace trace;
    int trial_avail[BANK_M];
} ReqResult;

/* kind_need 为真时，rows 是尚需，最大需求由已分配加出来。 */
int bank_bind(Bank *b, int kind_need, const int rows[][BANK_M]);
int bank_from_total(Bank *b, const int *total);

int bank_safe(const Bank *b, SafeTrace *out);
/* 走得通返回 1，中途卡住返回 0 并把 fail 设成该进程下标。序号不合法返回 -1。 */
int bank_check(const Bank *b, const int *order, int *fail);
int bank_collect(const Bank *b, int *total, int seqs[][BANK_N], int cap);
int bank_request(const Bank *b, int who, const int *req, ReqResult *out);

void bank_print_need(const Bank *b);
void bank_print_trace(const Bank *b, const SafeTrace *t);
void bank_print_check(const Bank *b, const int *order, int ok, int fail);
void bank_print_count(const Bank *b, int total, const int seqs[][BANK_N], int cap);
void bank_print_request(const Bank *b, int who, const int *req, const ReqResult *r);

#endif
