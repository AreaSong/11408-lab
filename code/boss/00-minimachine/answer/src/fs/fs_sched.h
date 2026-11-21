#ifndef FS_SCHED_H
#define FS_SCHED_H

#include <stdio.h>

#define FS_SCHED_N 32

typedef struct {
    int track[FS_SCHED_N];
    int index[FS_SCHED_N];
    int n;
    int move;
} FsSched;

/* dir：1 增大，-1 减小，0 只给 FCFS / SSTF。has_max 为 0 时不使用 max_track。 */
int fs_sched(const char *algo, int head, int dir, int has_max, int max_track, const int *req, int n,
             FsSched *out);
/* 磁头停在 50。每服务一次同磁道请求就再追加一次，用来看磁臂粘着。 */
int fs_stick(const char *algo, int extra, int *order, int *n, int *stuck);
void fs_sched_print(FILE *fp, const char *algo, const FsSched *got);

#endif
