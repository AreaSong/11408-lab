#ifndef PARALLEL_DEP_H
#define PARALLEL_DEP_H

/* 步骤 ①。调用方保证 n > 0，并且 dep_ilp 的 n 是 4 的倍数。数组内容对编译器不可见。 */
double dep_chain(const double *a, int n);
double dep_ilp(const double *a, int n);
double dep_array(const double *a, double *b, int n);

#endif
