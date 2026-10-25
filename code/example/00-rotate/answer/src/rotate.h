#ifndef ROTATE_H
#define ROTATE_H

/* 把 a[lo..hi]（闭区间）原地逆置 */
void reverse(int *a, int lo, int hi);

/* 把 a 的前 n 个元素原地循环左移 p 位；p 超出 [0, n) 时按 p mod n 处理 */
void rotate_left(int *a, int n, int p);

#endif
