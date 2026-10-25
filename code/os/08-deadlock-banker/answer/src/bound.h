#ifndef BOUND_H
#define BOUND_H

/* 保证不死锁的最少资源 = Σ(最大需求-1) + 1。k 相同就退化成 n*(k-1)+1。 */
int bound_equal(int n, int k, long long *least);
int bound_list(const int *k, int n, long long *least, long long *worst);
/* 资源数为 m、每人最多 k 个时，保证不死锁的最多进程数，以及可能死锁的最少进程数。 */
int bound_maxn(int m, int k, int *out);
int bound_mink(int m, int k, int *out);

#endif
