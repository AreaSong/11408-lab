#include "dep.h"

/*
 * 关掉向量化和展开：这一步要看的是标量指令流里的数据相关。
 * 若允许自动向量化，独立循环会变成步骤 ④ 的 SIMD，两件事叠在一起。
 */
double dep_chain(const double *a, int n) {
    double s = 1.0;
    int i;
#pragma clang loop vectorize(disable) interleave(disable) unroll(disable)
    for (i = 0; i < n; i++) {
        s = s * a[i];
    }
    return s;
}

double dep_ilp(const double *a, int n) {
    double s0 = 1.0;
    double s1 = 1.0;
    double s2 = 1.0;
    double s3 = 1.0;
    int i;
#pragma clang loop vectorize(disable) interleave(disable) unroll(disable)
    for (i = 0; i < n; i += 4) {
        s0 = s0 * a[i];
        s1 = s1 * a[i + 1];
        s2 = s2 * a[i + 2];
        s3 = s3 * a[i + 3];
    }
    return (s0 + s1) + (s2 + s3);
}

double dep_array(const double *a, double *b, int n) {
    int i;
#pragma clang loop vectorize(disable) interleave(disable) unroll(disable)
    for (i = 0; i < n; i++) {
        b[i] = a[i] * 1.0000001 + 1.0;
    }
    return b[0] + b[n / 2] + b[n - 1];
}
