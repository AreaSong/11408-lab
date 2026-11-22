#include "missmin.h"

#include <stdlib.h>

int find_miss_min(const int *a, int n) {
    int *mark;
    int ans;
    if (n < 1) {
        return 1;
    }
    if (a == NULL) {
        return -1;
    }
    /* 答案只可能是 1..n+1。mark[i] 表示正整数 i+1 出现过，越界和负数不用记。 */
    mark = calloc((size_t)n, sizeof(int));
    if (mark == NULL) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (a[i] > 0 && a[i] <= n) {
            mark[a[i] - 1] = 1;
        }
    }
    ans = n + 1;
    for (int i = 0; i < n; i++) {
        if (mark[i] == 0) {
            ans = i + 1;
            break;
        }
    }
    free(mark);
    return ans;
}
