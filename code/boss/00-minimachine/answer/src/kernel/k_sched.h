#ifndef K_SCHED_H
#define K_SCHED_H

#include <stdio.h>

/*
 * 单处理机调度计算器。时间是抽象单位，甘特图区间左闭右开。
 * 口径与 os/04、os/05 相同：同一时刻先接纳新进程，再处理时间片用完；
 * 剩余时间或优先级只在严格更优时抢占；响应比打平按先来先服务。
 * 多级反馈队列的每一级、以及 FCFS/RR 的就绪队列，都用 lib/04 的循环队列。
 */

#define K_JOB_MAX 8
#define K_SEG_MAX 64

enum {
    K_FCFS = 0,
    K_SJF,
    K_SRTN,
    K_HRRN,
    K_RR,
    K_PRIO_NP,
    K_PRIO_P,
    K_MLFQ,
    K_FB2,
    K_ALGO_N
};

enum { K_WHEN_EXIT = 1, K_WHEN_BLOCK, K_WHEN_SLICE, K_WHEN_ARRIVE };

typedef struct {
    char name[16];
    long long arrive;
    long long service;
    long long prio;
    long long io;
    long long cpu2;
} KJob;

typedef struct {
    char algo[32];
    char error[32];
    int nseg;
    long long seg_t0[K_SEG_MAX];
    long long seg_t1[K_SEG_MAX];
    char seg_name[K_SEG_MAX][16];
    int n;
    char jname[K_JOB_MAX][16];
    long long finish[K_JOB_MAX];
    long long turn[K_JOB_MAX];
    long long wait[K_JOB_MAX];
    long long avg_turn_n, avg_turn_d;
    long long avg_wait_n, avg_wait_d;
    long long avg_weight_n, avg_weight_d;
} KSchedOut;

const char *k_algo_name(int algo);
int k_algo_id(const char *name);
/* 这种算法遇到该事件时要不要调度。1 要，0 不要。 */
int k_when(int algo, int when);
int k_sched_run(KSchedOut *out, int algo, int quantum, int levels, int large, const KJob *jobs, int n);
void k_sched_print(const KSchedOut *out, FILE *fp);

#endif
