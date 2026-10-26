#ifndef PARALLEL_SHARE_H
#define PARALLEL_SHARE_H

typedef struct {
    long long ns;
    unsigned long long addr_a;
    unsigned long long addr_b;
    int value_a;
    int value_b;
} ShareResult;

/* apart_bytes 是两个 int 的距离，必须是 4 的正倍数。失败返回 -1。 */
int share_run(int apart_bytes, int times, ShareResult *out);

#endif
