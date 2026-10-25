#include "rotate.h"
#include "test.h"

/* 复制一份再左移，逐个比对；出错时打印的行号指向这里，看 actual/expected 的值定位是哪个元素 */
static void check_rotate(const int *in, int n, int p, const int *want) {
    int a[16];
    for (int i = 0; i < n; i++) {
        a[i] = in[i];
    }
    rotate_left(a, n, p);
    for (int i = 0; i < n; i++) {
        CHECK_EQ(a[i], want[i]);
    }
}

int main(void) {
    const int seq[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    const int left3[] = {3, 4, 5, 6, 7, 8, 9, 0, 1, 2};
    check_rotate(seq, 10, 3, left3);

    /* p = 0 和 p = n-1 是两个端点：前者不动，后者相当于右移 1 位 */
    check_rotate(seq, 10, 0, seq);
    const int left9[] = {9, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    check_rotate(seq, 10, 9, left9);

    /* p 超出范围按 p mod n 处理：左移 13 位等于左移 3 位 */
    check_rotate(seq, 10, 13, left3);

    const int one[] = {42};
    check_rotate(one, 1, 0, one);

    int r[] = {1, 2, 3, 4};
    reverse(r, 1, 3);
    CHECK_EQ(r[0], 1);
    CHECK_EQ(r[1], 4);
    CHECK_EQ(r[3], 2);

    return TEST_SUMMARY();
}
