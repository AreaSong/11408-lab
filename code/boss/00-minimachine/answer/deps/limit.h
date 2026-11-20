#ifndef LIMIT_H
#define LIMIT_H

/* 阶数 3..16，高度 1..12。高度只数含关键字的层，失败结点不算。 */
int limit_order(int m);
int limit_ceil_half(int m);
int limit_min_keys(int m, int is_root);

/*
 * 最少关键字 2*⌈m/2⌉^(h-1)-1，最多 m^h-1。
 * 结点数按同样的最瘦、最胖树形数。成功返回 1。
 */
int limit_bounds(int m, int h, long long *min_k, long long *max_k,
                 long long *min_n, long long *max_n);

/* n 个关键字能摆出的含关键字结点个数的最大值。 */
int limit_max_nodes(int n, int m, long long *out);

/* 高度 h、第 2 层恰好 keys 个关键字时，含关键字结点个数的最大值。 */
int limit_most(int m, int h, int keys, long long *out);

/* n 个有序关键字能组成的不同 m 阶 B 树棵数。n 不超过 12。 */
int limit_shapes(int n, int m, long long *out);

#endif
