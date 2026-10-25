#include "majority_element.h"

int majority_element(const int a[], int n, FILE *trace) {
    if (n <= 0 || a == NULL) {
        return -1;
    }
    /* 王道的候选计数：不同就减 1，减到 0 时当前这个元素自己成为新候选的第一票。
     * 主元素若存在，和别的值两两抵消后仍有剩余，最后留下的候选只能是它。
     * 不存在时也会留下某个候选，所以后面必须再数一遍真实次数。 */
    int candidate = a[0];
    int count = 1;
    if (trace != NULL) {
        fprintf(trace, "pick candidate=%d count=1\n", candidate);
    }
    for (int i = 1; i < n; i++) {
        if (a[i] == candidate) {
            count++;
            if (trace != NULL) {
                fprintf(trace, "i=%d a=%d vote count=%d\n", i, a[i], count);
            }
        } else {
            count--;
            if (count == 0) {
                candidate = a[i];
                count = 1;
                if (trace != NULL) {
                    fprintf(trace, "i=%d a=%d replace candidate=%d count=1\n", i, a[i], candidate);
                }
            } else if (trace != NULL) {
                fprintf(trace, "i=%d a=%d cancel count=%d\n", i, a[i], count);
            }
        }
    }
    int times = 0;
    for (int i = 0; i < n; i++) {
        if (a[i] == candidate) {
            times++;
        }
    }
    if (trace != NULL) {
        fprintf(trace, "verify candidate=%d times=%d\n", candidate, times);
    }
    /* n/2 是整数除法。times > n/2 与 times * 2 > n 等价；写成 >= 会把“恰好一半”算进去 */
    if (times > n / 2) {
        return candidate;
    }
    return -1;
}
