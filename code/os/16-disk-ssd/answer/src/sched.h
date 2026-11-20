#ifndef SCHED_H
#define SCHED_H

#define SCHED_N 32

typedef struct {
    int track[SCHED_N];
    int index[SCHED_N];
    int n;
    int move;
} SchedOut;

/* dir：1 增大，-1 减小，0 只给 FCFS / SSTF。has_max 为 0 时不使用 max_track。 */
int sched_run(const char *algo, int head, int dir, int has_max, int max_track,
              const int *req, int n, SchedOut *out);

/* 磁头停在 50，队列里先有 50 再有 80。每服务一次 50 就再追加一个 50，最多 extra 次。 */
int stick_run(const char *algo, int extra, int *order, int *n, int *stuck);
void sched_print(const char *algo, const SchedOut *out);

#endif
