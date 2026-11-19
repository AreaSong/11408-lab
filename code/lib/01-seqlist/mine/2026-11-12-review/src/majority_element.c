#include "majority_element.h"

// int a1[] = {0, 5, 5, 3, 5, 7, 5, 5};
// CHECK_EQ(majority_element(a1,8),5);

int majority_element(int A[], int n) {
    int cand = 0, count = 0;
    for (int i = 0; i < n; i++) {
        if (count == 0) {
            cand = A[i];
            count = 1;
        } else if (A[i] == cand)
            count++;
        else
            count--;
    }

    int times = 0;
    for (int i = 0; i < n; i++)
        if (A[i] == cand)
            times++;
    if (times > n / 2)
        return cand;
    return -1;
}
