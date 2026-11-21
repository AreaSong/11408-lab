#ifndef FS_GEOM_H
#define FS_GEOM_H

#include <stdio.h>

typedef struct {
    long long seek;
    long long rot;
    long long xfer;
    long long ctrl;
} FsTime;

long long fs_disk_bytes(int cylinders, int surfaces, int sectors, int sec_bytes);
/* 簇号、柱面、磁头、扇区都从 0 起。扇区号是这个簇的第一个扇区。 */
int fs_cluster_chs(int surfaces, int sectors, int cluster_secs, int cluster, int *cyl, int *head,
                   int *sector);
int fs_cluster_sstf(int surfaces, int sectors, int cluster_secs, int head_cyl, const int *clusters,
                    int n, int *order);

/* 每次访问都付一次寻道、半圈、一个扇区和控制器延迟。刻度是 1 微秒 = 3000。 */
int fs_time_each(long long seek_us, int rpm, int sectors, int count, long long ctrl_us, FsTime *out);
/* seek_us 已经是这批请求的寻道总时间。 */
int fs_time_sum(long long seek_us, int rpm, int sectors, int count, long long ctrl_us, FsTime *out);
/* 1 KB = 10^3 B，1 MB = 10^6 B，对齐 2013 年按十进制相除的 0.2 ms。 */
int fs_time_rate(long long seek_us, int rpm, long long bytes, long long bps, long long ctrl_us,
                 FsTime *out);
int fs_fmt_ms(long long ticks, int ndp, char *buf, int cap);
void fs_time_print(FILE *fp, const FsTime *parts);
/* 交叉因子：步长必须和每磁道扇区数互素，时间单位是转过一个扇区。 */
int fs_layout(int nsec, int gap, int nread, int proc, int *ticks, int *wait, int *proc_sum);

#endif
