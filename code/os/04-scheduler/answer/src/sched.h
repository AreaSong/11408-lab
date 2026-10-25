#ifndef SCHED_H
#define SCHED_H

/*
 * 单处理机调度。时间是整数，甘特图区间左闭右开：[start, end) 上是该名字。
 * 周转 = 完成 − 到达，带权周转 = 周转 / 服务，等待 = 周转 − 服务。
 * 本关进程是纯 CPU，没有单独的 I/O 时间。切换开销算进周转和等待，不算服务。
 * 到达时刻为负表示 0 时刻之前已经等过，用来做「T 时刻已经在就绪队列」的题。
 */

#define SCHED_NAME 16
#define SCHED_MAX 16
#define SCHED_SEG_MAX 512
#define SCHED_TRACE_MAX 128
#define SCHED_TRACE_LEN 512

typedef enum {
    SCHED_FCFS = 0,
    SCHED_SJF,
    SCHED_SRTN,
    SCHED_HRRN,
    SCHED_RR,
    SCHED_PRIO_NP,
    SCHED_PRIO_P,
    SCHED_ALGO_N
} SchedAlgo;

typedef enum {
    PRIO_NONE = 0,
    PRIO_LARGE,
    PRIO_SMALL
} PrioOrder;

enum {
    SCHED_DISPATCH = -1,
    SCHED_IDLE = -2
};

typedef struct {
    char name[SCHED_NAME];
    long long arrive;
    long long service;
    long long prio;
} SchedJob;

typedef struct {
    long long start;
    long long end;
    int kind;
} SchedSeg;

typedef struct {
    long long start;
    long long finish;
    long long turn;
    long long wait;
    long long response;
} SchedStat;

typedef struct {
    SchedAlgo algo;
    long long quantum;
    long long switch_cost;
    PrioOrder order;
    long long at;
    int n;
    SchedJob jobs[SCHED_MAX];
    int nseg;
    SchedSeg segs[SCHED_SEG_MAX];
    SchedStat stats[SCHED_MAX];
    int ntrace;
    char trace[SCHED_TRACE_MAX][SCHED_TRACE_LEN];
    int chosen;
    int nready;
    int ready_ids[SCHED_MAX];
    char error[32];
} SchedResult;

int sched_run(SchedResult *out, SchedAlgo algo, long long quantum, long long switch_cost,
              PrioOrder order, const SchedJob *jobs, int n);
int sched_pick(SchedResult *out, SchedAlgo algo, long long at, PrioOrder order,
               const SchedJob *jobs, int n);

#endif
