#ifndef DEV_BUF_H
#define DEV_BUF_H

/*
 * T 是设备把一块送进缓冲区的时间，M 是搬到用户区的时间，C 是 CPU 处理的时间。
 * 同一块缓冲区不能同时被写入和被搬走。用户区同一时刻只放一块。
 * 单缓冲要等搬运结束才能送下一块。双缓冲可以往另一块送。
 */
typedef struct {
    int block;
    int buf;
    long long in0, in1;
    long long mv0, mv1;
    long long cpu0, cpu1;
} DevBufStep;

long long dev_buf_finish(int dual, int n, long long t, long long m, long long c, DevBufStep *out);
long long dev_buf_closed(int dual, int n, long long t, long long m, long long c);

#endif
