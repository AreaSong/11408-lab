#include "rotate.h"

void reverse(int *a, int lo, int hi) {
    while (lo < hi) {
        int t = a[lo];
        a[lo++] = a[hi];
        a[hi--] = t;
    }
}

/*
 * 三次逆置：把序列看成 AB，A 是前 p 个、B 是后 n-p 个，目标是 BA。
 * 先分别逆置得到 A⁻¹B⁻¹，再整体逆置得到 (A⁻¹B⁻¹)⁻¹ = BA。
 * 每个元素被交换的次数是常数，所以时间 O(n)；只用了一个临时变量，额外空间 O(1)。
 */
void rotate_left(int *a, int n, int p) {
    if (n <= 1) {
        return;
    }
    p %= n;
    if (p < 0) {
        p += n;   /* C 里负数取模结果为负，补回到 [0, n) */
    }
    if (p == 0) {
        return;
    }
    reverse(a, 0, p - 1);
    reverse(a, p, n - 1);
    reverse(a, 0, n - 1);
}
