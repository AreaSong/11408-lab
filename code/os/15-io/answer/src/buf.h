#ifndef BUF_H
#define BUF_H

/* 一块数据在时间线上的三段：设备输入、搬到用户区、CPU 处理。 */
typedef struct {
    int block;
    int buf;
    long long in0, in1;
    long long mv0, mv1;
    long long cpu0, cpu1;
} BufStep;

long long buf_finish(int dual, int n, long long t, long long m, long long c, BufStep *out);
long long buf_closed(int dual, int n, long long t, long long m, long long c);
int buf_report(int which, int trace, int n, long long t, long long m, long long c);

#endif
