#ifndef TIME_H
#define TIME_H

typedef struct {
    long long seek;
    long long rot;
    long long xfer;
    long long ctrl;
} TimeParts;

/* 每次访问都付一次寻道、半圈、一个扇区和控制器延迟。刻度是 1/3 纳秒。 */
int time_each(long long seek_us, int rpm, int sectors, int count, long long ctrl_us,
              TimeParts *out);

/* seek_us 已经是这批请求的寻道总时间，旋转和传输按 count 次累加。 */
int time_sum(long long seek_us, int rpm, int sectors, int count, long long ctrl_us,
             TimeParts *out);

/* 传输时间 = bytes / bps。1 KB = 10^3 B，1 MB = 10^6 B，用来对齐 2013 的 0.2ms。 */
int time_rate(long long seek_us, int rpm, long long bytes, long long bps, long long ctrl_us,
              TimeParts *out);

int fmt_ms(long long ticks, int ndp, char *buf, int cap);
void time_print(const TimeParts *parts);

/* 步长 = gap + 1，必须和 nsec 互素。时间单位是「转过一个扇区」。 */
int layout_run(int nsec, int gap, int nread, int proc, int *ticks, int *wait, int *proc_sum);

#endif
