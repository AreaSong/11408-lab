#ifndef MLFQ_INT_H
#define MLFQ_INT_H

#include "mlfq.h"

/* 模拟器内部状态。队列里放的是 jobs 的下标，正在运行的进程不在队列中。 */
typedef struct {
    int id[MLFQ_MAX];
    int n;
} MlfqQueue;

typedef struct {
    char name[16];
    long long arrive, cpu1, io, cpu2;
    long long remain, seg, wake, start, finish, used;
    int phase, level, seq, admitted, queued, blocked, done;
} MlfqProc;

typedef struct {
    MlfqResult *out;
    MlfqProc jobs[MLFQ_MAX];
    MlfqQueue q[MLFQ_LEVELS];
    int n, running, seq, admitted_now, woke_now;
    long long now, run_from, limit;
    char run_name[16];
    char last[16];
} MlfqSim;

int mlfq_drive(MlfqSim *sim);

#endif
