#ifndef PARALLEL_CORES_H
#define PARALLEL_CORES_H

/*
 * 步骤 ②。n 必须能被 nthread 整除，1 ≤ nthread ≤ 8。
 * 返回并行区的墙钟纳秒。失败返回 -1。*checksum 取 b[0]，各核算的是同一公式。
 */
long long cores_run(const double *a, double *b, int n, int nthread, int rounds, int passes,
                    double *checksum);

#endif
