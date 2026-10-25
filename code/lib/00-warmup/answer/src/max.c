#include "max.h"

/* 时间 O(n)：每个元素恰好比较一次；空间 O(1)：只多用了一个变量 best。 */
bool max_find(const int a[], int n, int *out) {
    if (n <= 0) {
        return false;
    }
    /* 用 a[0] 而不是 0 当初值：全是负数时，初值 0 会得到错误的最大值 0 */
    int best = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] > best) {
            best = a[i];
        }
    }
    *out = best;
    return true;
}
