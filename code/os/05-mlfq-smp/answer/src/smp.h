#ifndef SMP_H
#define SMP_H

#define SMP_MAX 8
#define SMP_CPU 2
#define SMP_SEG 48
#define SMP_NONE 0
#define SMP_SOFT 1
#define SMP_HARD 2

typedef struct {
    char name[16];
    long long arrive;
    long long service;
    int affinity;
} SmpJob;

typedef struct {
    long long arrive;
    long long service;
    int affinity;
    long long start;
    long long finish;
    int moves;
    char cpus[32];
} SmpStat;

typedef struct {
    char mode[12];
    char balance[12];
    char affinity[12];
    long long slice;
    int migrations;
    long long makespan;
    int nseg[SMP_CPU];
    long long t0[SMP_CPU][SMP_SEG];
    long long t1[SMP_CPU][SMP_SEG];
    char gname[SMP_CPU][SMP_SEG][16];
    SmpStat stats[SMP_MAX];
    char jname[SMP_MAX][16];
    int n;
    char error[32];
} SmpResult;

int smp_run(int shared, int balance, int aff, long long slice, const SmpJob *jobs, int n,
            SmpResult *out);
void smp_print(const SmpResult *out);
int smp_compare(long long slice, int aff, const SmpJob *jobs, int n);

#endif
