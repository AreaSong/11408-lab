#ifndef MLFQ_H
#define MLFQ_H

#define MLFQ_MAX 8
#define MLFQ_LEVELS 4
#define MLFQ_TRACE 96
#define MLFQ_SEG 64
#define MLFQ_LINE 128

typedef struct {
    char name[16];
    long long arrive;
    long long cpu1;
    long long io;
    long long cpu2;
} MlfqJob;

typedef struct {
    char algo[8];
    int levels;
    int base_q;
    int quantum[MLFQ_LEVELS];
    int sjf[MLFQ_LEVELS];
} MlfqSpec;

typedef struct {
    long long arrive;
    long long service;
    long long start;
    long long finish;
    long long turn;
    long long wait;
    long long response;
    int level;
} MlfqStat;

typedef struct {
    char algo[8];
    int levels;
    int base_q;
    int quantum[MLFQ_LEVELS];
    int sjf[MLFQ_LEVELS];
    char trace[MLFQ_TRACE][MLFQ_LINE];
    int ntrace;
    char gname[MLFQ_SEG][16];
    long long gt0[MLFQ_SEG];
    long long gt1[MLFQ_SEG];
    int nseg;
    MlfqStat stats[MLFQ_MAX];
    char jname[MLFQ_MAX][16];
    int n;
    char error[32];
} MlfqResult;

int mlfq_standard(MlfqSpec *spec, int levels, long long q);
void mlfq_fb2(MlfqSpec *spec);
int mlfq_run(const MlfqSpec *spec, const MlfqJob *jobs, int n, MlfqResult *out);
void mlfq_print(const MlfqResult *out);

#endif
